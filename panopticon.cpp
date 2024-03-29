// Copyright (C) 2015 Philip Cronje. All rights reserved.
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

#include "cloudwatch.h"
#include "epoll.h"
#include "net.h"
#include "ssl.h"
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
		constexpr static const char* envVarAccessKey = "AWS_ACCESS_KEY_ID";
		std::string accessKey;

		constexpr static const char* envVarHostName = "PANOPTICON_CLOUDWATCH_HOST";
		constexpr static const char* defaultHostName = "monitoring.us-east-1.amazonaws.com";
		std::string cloudWatchHostName = defaultHostName;

		constexpr static const char* defaultRegion = "us-east-1";
		std::string region = defaultRegion;

		constexpr static const char* envVarSecretKey = "AWS_SECRET_KEY";
		std::string secretKey;

		arguments() {
			bool overrideRegion = false;
			const char* region_ = std::getenv("PANOPTICON_REGION");
			if(region_ != nullptr) {
				region = region_;
				overrideRegion = true;
			}

			const char* cloudWatchHostName_ = std::getenv(envVarHostName);
			if(cloudWatchHostName_ != nullptr) {
				cloudWatchHostName = cloudWatchHostName_;
			} else if(overrideRegion) {
				cloudWatchHostName = "monitoring." + region + ".amazonaws.com";
			}

			const char* accessKey_ = std::getenv(envVarAccessKey);
			if(accessKey_ != nullptr) accessKey = accessKey_;
			const char* secretKey_ = std::getenv(envVarSecretKey);
			if(secretKey_ != nullptr) secretKey = secretKey_;
		}
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

	void parseAwsCredentials(arguments& arguments, std::istream& file) {
		std::string profile;
		for(std::string line; std::getline(file, line);) {
			if(line[0] == '[' && line[line.size() - 1] == ']') {
				profile = line.substr(1, line.size() - 2);
			} else if(profile == "default") {
				auto i = line.find_first_of('=');
				if(i == std::string::npos) {
					continue;
				}

				int lws, tws;
				for(lws = i; line[lws] == ' '; --lws);
				for(tws = i + 1; line[tws] == ' '; ++tws);
				std::string key = line.substr(0, lws - 1);
				std::string value = line.substr(tws);
				if((key == "aws_access_key_id") && arguments.accessKey.empty()) {
					arguments.accessKey = value;
				} else if((key == "aws_secret_access_key") && arguments.secretKey.empty()) {
					arguments.secretKey = value;
				}
			}
		}
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

		/* Derive the CloudWatch host name if a region parameter was supplied, and no host name was supplied as either
		 * a command line argument or an environment variable. */
		if(overrideRegion && !overrideCloudWatchHostName && (std::getenv(arguments::envVarHostName) == nullptr)) {
			arguments.cloudWatchHostName = "monitoring." + arguments.region + ".amazonaws.com";
		}

		if(arguments.accessKey.empty() || arguments.secretKey.empty()) {
			const char* homeEnvVar = std::getenv("HOME");
			if(homeEnvVar != nullptr) {
				std::string home = std::string(homeEnvVar) + "/.aws/credentials";
				std::ifstream file(home);
				if(file.is_open()) {
					parseAwsCredentials(arguments, file);
				}
			}
		}

		return arguments;
	}

	std::string getLocalHostName() {
		const long hostNameMax = sysconf(_SC_HOST_NAME_MAX);
		if(hostNameMax == -1) {
			throw std::system_error(errno, std::system_category(), "Failed to retrieve maximum host name length");
		}

		char localHostName[hostNameMax];
		if(gethostname(localHostName, sizeof(localHostName)) == -1) {
			throw std::system_error(errno, std::system_category(), "Failed to retrieve local host name");
		}
		return localHostName;
	}

	void main(const std::string& executable, const argumentsContainer& argv) {
		const arguments arguments = parseArguments(executable, argv);

		const std::string localHostName = getLocalHostName();

		std::signal(SIGINT, handleSignal);

		ssl::library sslLibrary;

		epoll epoll;

		auto cpu = std::make_pair(stat::cpu(), stat::cpu(std::ifstream("/proc/stat")));
		using cpuAggregation = stat::aggregation<double>;
		cpuAggregation user, system, ioWait;

		const ssl::context sslContext;
		sslContext.setDefaultVerifyPaths();
		std::unique_ptr<ssl::connection> sslConnection;

		util::buffer requestBuffer;

		net::socket socket;
		while(signalStatus == 0) {
			using clock = std::chrono::steady_clock;
			auto now = clock::now();
			const auto then = now + std::chrono::seconds(1);

			swapIn(cpu, stat::cpu(std::ifstream("/proc/stat")));
			cpu.second.aggregate(cpu.first, user, system, ioWait);

			if(countExceeded(60, user, system, ioWait)) {
				requestBuffer = aws::cloudwatch::newPutMetricDataRequest(arguments.cloudWatchHostName,
						arguments.region, arguments.accessKey, arguments.secretKey, "Panopticon",
						{ { "Host", localHostName } },
						{ { "UserCPU", user }, { "SystemCPU", system }, { "IOWaitCPU", ioWait } });
				std::cout << "Request buffer is " << requestBuffer.size() << " bytes" << std::endl;
				if(!(socket.connected() || socket.connecting())) {
					socket.connect(arguments.cloudWatchHostName, 443, epoll);
					sslConnection.reset(new ssl::connection(sslContext, socket));
				}
				user = system = ioWait = cpuAggregation();
			}

			while(((now = clock::now()) < then) && (signalStatus == 0)) {
				epoll_event event { 0, nullptr };
				if(!epoll.wait(event, then - now)
						&& !socket.readable() && !socket.writable() && !requestBuffer) {
					continue;
				}

				if((event.events & EPOLLOUT) != 0) socket.writable(true);
				if((event.events & EPOLLIN) != 0) socket.readable(true);

				std::cout << "SSL state: " << std::hex << SSL_get_state(*sslConnection.get()) << std::dec <<
					" - Socket state (W/R): " << socket.writable() << "/" << socket.readable() <<
					" - Request buffer state: " << static_cast<bool>(requestBuffer) << std::endl;

				if(socket.connecting() && socket.writable()) {
					socket.completeConnect();
				} else if(socket.connected()) {
					if(socket.readable()) sslConnection->readFromSocket();
					if(socket.writable()) sslConnection->writeToSocket();
				}

				if(sslConnection->inConnectInit()) {
					if(!sslConnection->connect()) {
						continue;
					}

					ssl::x509 certificate = sslConnection->peerCertificate();
					if(!certificate) {
						throw std::logic_error("No peer certificate presented");
					} else {
						sslConnection->verifyPeerCertificate();
						if(!certificate.matchSan(arguments.cloudWatchHostName)) {
							if(!certificate.matchCommonName(arguments.cloudWatchHostName)) {
								throw std::logic_error("SAN/CN mismatch");
							}
						}
					}
				} else if(sslConnection->initFinished()) {
					if(requestBuffer) sslConnection->write(requestBuffer);
					sslConnection->read();
				}
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
