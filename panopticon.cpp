// Copyright (C) 2015 Philip Cronje. All rights reserved.
#include <chrono>
#include <csignal>
#include <fstream>
#include <utility>

#include "epoll.h"
#include "stat-cpu.h"

namespace {
	volatile std::sig_atomic_t signalStatus = 0;
	void handleSignal(int signal) { signalStatus = signal; }

	template<typename A>
	void swapIn(std::pair<A, A>& pair, A&& value) {
		pair.first = std::move(pair.second);
		pair.second = std::move(value);
	}
}

int main(int argc, char** argv) {
	std::signal(SIGINT, handleSignal);

	epoll epoll;

	auto cpu = std::make_pair(stat::cpu(), stat::cpu(std::ifstream("/proc/stat")));

	while(signalStatus == 0) {
		using clock = std::chrono::steady_clock;
		auto now = clock::now();
		const auto then = now + std::chrono::seconds(1);

		swapIn(cpu, stat::cpu(std::ifstream("/proc/stat")));

		while(((now = clock::now()) < then) && (signalStatus == 0)) {
			epoll_event event;
			if(!epoll.wait(event, then - now)) continue;
		}
	}
	return 0;
}
