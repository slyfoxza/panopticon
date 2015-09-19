// Copyright (C) 2015 Philip Cronje. All rights reserved.
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

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

	template<typename T, typename V>
	constexpr bool matchesAny(const T& value, const V& v) { return v == value; }

	template<typename T, typename V0, typename... VT>
	constexpr bool matchesAny(const T& value, const V0& v0, const VT... vt) {
		return matchesAny(value, v0) || matchesAny(value, vt...);
	}

	struct arguments {
		constexpr static const char* defaultHostName = "monitoring.us-east-1.amazonaws.com";
		std::string cloudWatchHostName = defaultHostName;

		constexpr static const char* defaultRegion = "us-east-1";
		std::string region = defaultRegion;
	};

	class argumentsHelp {};
	class missingArgumentValue {
		const std::string argument_;

		public:
			missingArgumentValue(const std::string& argument) noexcept : argument_(argument) {}
			std::string argument() const noexcept { return argument_; }
	};

	void printHelp(const std::string& executable) {
		std::cout << "Usage: " << executable << " [options]\n\n"
			"Options:\n"
			"-H --host <host>\n\tAWS CloudWatch hostname (default: " << arguments::defaultHostName << ")\n"
			"-r --region <region>\n\tAWS region (default: " << arguments::defaultRegion << ")\n"
			"-h -? --help\n\tPrints this message" << std::endl;
	}

	template<typename I>
	bool assertHasOption(const std::string& argument, I& iterator, const I& end) {
		if(++iterator == end) throw missingArgumentValue(argument);
		return true;
	}

	typedef std::vector<std::string> argumentsContainer;
	arguments parseArguments(const std::string& executable, const argumentsContainer& argv) {
		arguments arguments;
		bool overrideRegion = false;
		bool overrideCloudWatchHostName = false;
		for(auto it = argv.cbegin(); it != argv.cend(); ++it) {
			const std::string& argument = *it;
			if(matchesAny(argument, "-H", "--host") && assertHasOption(argument, it, argv.cend())) {
				arguments.cloudWatchHostName = *it;
				overrideCloudWatchHostName = true;
			} else if(matchesAny(argument, "-r", "--region") && assertHasOption(argument, it, argv.cend())) {
				arguments.region = *it;
				overrideRegion = true;
			} else if(matchesAny(argument, "-h", "-?", "--help")) {
				printHelp(executable);
				throw argumentsHelp();
			}
		}

		if(overrideRegion && !overrideCloudWatchHostName) {
			arguments.cloudWatchHostName = "monitoring." + arguments.region + ".amazonaws.com";
		}

		return arguments;
	}

	void main(const std::string& executable, const argumentsContainer& argv) {
		const arguments arguments = parseArguments(executable, argv);

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
	}
}

int main(int argc, char** argv) {
	try {
		main(argv[0], argumentsContainer(argv + 1, argv + argc));
		return 0;
	} catch(const missingArgumentValue& e) {
		std::cerr << "Missing argument value for " << e.argument() << std::endl;
		return 1;
	} catch(const argumentsHelp&) {
		return 0;
	}
}
