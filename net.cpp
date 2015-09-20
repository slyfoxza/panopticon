// Copyright (C) 2015 Philip Cronje. All rights reserved.
#include <netdb.h>

#include "net.h"

namespace {
	struct getaddrinfoCategory_ : public std::error_category {
		const char* name() const noexcept override { return "getaddrinfo"; }
		std::string message(int code) const noexcept override { return gai_strerror(code); }
	};

	getaddrinfoCategory_ getaddrinfoCategory;
}

void net::socket::connect(const char* hostName, const int port, const epoll& epoll) {
	const addrinfo hint {
		AI_NUMERICSERV | AI_V4MAPPED | AI_ADDRCONFIG, AF_UNSPEC, SOCK_STREAM, 0/*ai_protocol*/,
		0, nullptr, nullptr, nullptr
	};

	addrinfo* addresses;
	int rc = getaddrinfo(hostName, std::to_string(port).c_str(), &hint, &addresses);
	if(rc != 0) {
		if(rc == EAI_SYSTEM) {
			throw std::system_error(errno, std::system_category(), "Failed to resolve host");
		} else {
			throw std::system_error(rc, getaddrinfoCategory, "Failed to resolve host");
		}
	}

	int family;
	for(addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
		if((fd_ != -1) && (family != address->ai_family)) {
			close(fd_);
			fd_ = -1;
		}

		if(fd_ == -1) {
			fd_ = ::socket(address->ai_family, address->ai_socktype | SOCK_NONBLOCK, address->ai_protocol);
			if(fd_ == -1) {
				freeaddrinfo(addresses);
				throw std::system_error(errno, std::system_category(), "Failed to create socket");
			}
		}

		epoll += fd_;
		rc = ::connect(fd_, address->ai_addr, address->ai_addrlen);
		if(rc == 0) {
			connecting_ = false;
			connected_ = true;
			break;
		} else if(errno == EINPROGRESS) {
			connecting_ = true;
			connected_ = false;
			break;
		} else {
			epoll -= fd_;
		}
	}
	freeaddrinfo(addresses);
}

void net::socket::completeConnect() {
	int value = 0;
	socklen_t length = 0;
	const int rc = getsockopt(fd_, SOL_SOCKET, SO_ERROR, &value, &length);
	if(rc == -1) {
		throw std::system_error(errno, std::system_category(), "Failed to get socket error code");
	}

	if(value == 0) {
		connecting_ = false;
		connected_ = true;
	} else {
		throw std::system_error(value, std::system_category(), "Asynchronous connection failed");
	}
}
