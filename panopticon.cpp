// Copyright (C) 2015 Philip Cronje. All rights reserved.
#include <chrono>
#include <csignal>

#include "epoll.h"

namespace {
	volatile std::sig_atomic_t signalStatus = 0;
	void handleSignal(int signal) { signalStatus = signal; }
}

int main(int argc, char** argv) {
	epoll epoll;
	std::signal(SIGINT, handleSignal);
	while(signalStatus == 0) {
		using clock = std::chrono::steady_clock;
		auto now = clock::now();
		const auto then = now + std::chrono::seconds(1);

		while(((now = clock::now()) < then) && (signalStatus == 0)) {
			epoll_event event;
			if(!epoll.wait(event, then - now)) continue;
		}
	}
	return 0;
}
