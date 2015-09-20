// Copyright (C) 2015 Philip Cronje. All rights reserved.
#pragma once

#include <chrono>
#include <system_error>

#include <sys/epoll.h>
#include <unistd.h>

class epoll {
	const int fd_;

	public:
		epoll() : fd_(epoll_create1(0)) {
			if(fd_ == -1) throw std::system_error(errno, std::system_category(),
					"Failed to create epoll file descriptor");
		}
		~epoll() noexcept { close(fd_); }

		operator int() const noexcept { return fd_; }

		const epoll& operator+=(const int fd) const {
			epoll_event event { EPOLLIN | EPOLLOUT | EPOLLET, reinterpret_cast<void*>(fd) };
			const int rc = epoll_ctl(fd_, EPOLL_CTL_ADD, fd, &event);
			if(rc == -1) {
				throw std::system_error(errno, std::system_category(),
						"Failed to register file descriptor with epoll");
			}
			return *this;
		}

		const epoll& operator-=(const int fd) const {
			const int rc = epoll_ctl(fd_, EPOLL_CTL_DEL, fd, nullptr);
			if(rc == -1) {
				throw std::system_error(errno, std::system_category(), "Failed to remove file descriptor from epoll");
			}
			return *this;
		}

		template<typename D>
		bool wait(epoll_event& event, const D& timeout) const {
			const int rc = epoll_wait(fd_, &event, 1,
					std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
			if((rc == 0) || (errno == EINTR)) {
				return false;
			} else if(rc == -1) {
				throw std::system_error(errno, std::system_category(), "Failed to poll I/O events");
			} else {
				return true;
			}
		}
};
