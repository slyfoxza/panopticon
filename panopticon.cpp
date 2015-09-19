// Copyright (C) 2015 Philip Cronje. All rights reserved.
#include <chrono>
#include <csignal>
#include <fstream>
#include <utility>

#include "epoll.h"
#include "stat.h"
#include "stat-cpu.h"

namespace {
	volatile std::sig_atomic_t signalStatus = 0;
	void handleSignal(int signal) { signalStatus = signal; }

	template<typename A>
	void swapIn(std::pair<A, A>& pair, A&& value) {
		pair.first = std::move(pair.second);
		pair.second = std::move(value);
	}

	template<typename C, typename A>
	constexpr bool countExceeded(const C count, const A& a) { return a.count >= count; }

	template<typename C, typename A0, typename... AT>
	constexpr bool countExceeded(const C count, const A0& a0, const AT... at) {
		return countExceeded(count, a0) || countExceeded(count, at...);
	}
}

int main(int argc, char** argv) {
	std::signal(SIGINT, handleSignal);

	epoll epoll;

	auto cpu = std::make_pair(stat::cpu(), stat::cpu(std::ifstream("/proc/stat")));
	using cpuAggregation = stat::aggregation<double>;
	cpuAggregation user, system, ioWait;

	while(signalStatus == 0) {
		using clock = std::chrono::steady_clock;
		auto now = clock::now();
		const auto then = now + std::chrono::seconds(1);

		swapIn(cpu, stat::cpu(std::ifstream("/proc/stat")));
		cpu.second.aggregate(cpu.first, user, system, ioWait);

		if(countExceeded(60, user, system, ioWait)) {
			// TODO: Convert to PutMetricData request, ensure TLS connection exists, begin writing to AWS
			user = system = ioWait = cpuAggregation();
		}

		while(((now = clock::now()) < then) && (signalStatus == 0)) {
			epoll_event event;
			if(!epoll.wait(event, then - now)) continue;

			// TODO: Socket I/O
		}
	}
	return 0;
}
