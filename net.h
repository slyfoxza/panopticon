// Copyright (C) 2015 Philip Cronje. All rights reserved.
#pragma once

#include <unistd.h>

#include "epoll.h"

namespace net {
	class socket {
		bool connected_ = false;
		bool connecting_ = false;
		bool readable_ = false;
		bool writable_ = false;
		int fd_ = -1;

		public:
			socket() = default;
			socket(const socket&) = delete;
			~socket() noexcept { if(fd_ != -1) close(fd_); }

			operator int() const noexcept { return fd_; }
			bool connected() const noexcept { return connected_; }
			bool connecting() const noexcept { return connecting_; }

			bool readable() const noexcept { return readable_; }
			void readable(bool readable) noexcept { readable_ = readable; }
			bool writable() const noexcept { return writable_; }
			void writable(bool writable) noexcept { writable_ = writable; }

			void connect(const char* hostName, const int port, const epoll& epoll);
			void connect(const std::string& hostName, const int port, const epoll& epoll) {
				connect(hostName.c_str(), port, epoll);
			}

			void completeConnect();
	};
}
