// Copyright (C) 2015 Philip Cronje. All rights reserved.
#include <fstream>
#include <sstream>

#include "test-framework.h"

#include "stat-cpu.h"

namespace {
	void diagnose(const stat::cpu& cpu, const std::string& description) {
		std::cout << "# " << description << ":\n"
			"# Total  : " << cpu.total << "\n"
			"# User   : " << cpu.user << "\n"
			"# System : " << cpu.system << "\n"
			"# IO Wait: " << cpu.ioWait << std::endl;
	}

	bool parseProcStat() {
		const stat::cpu cpu = stat::cpu(std::ifstream("/proc/stat"));
		diagnose(cpu, "CPU stats from live /proc/stat");
		return true;
	}

	bool parseReordered() {
		const stat::cpu cpu = stat::cpu(std::ifstream("test-stat-cpu.reordered"));
		diagnose(cpu, "Re-ordered CPU stats from test-stat-cpu.reordered");
		return (cpu.total == (705754 + 914 + 64367 + 16413999 + 40284 + 5 + 9943))
			&& (cpu.user == (705754 + 914))
			&& (cpu.system == 64367)
			&& (cpu.ioWait == 40284);
	}

	bool parseSemiJunk() {
		const stat::cpu cpu = stat::cpu(std::istringstream("cpu  100 10 50 1000 200 junk this is all junk 999"));
		diagnose(cpu, "Semi-junk CPU metrics");
		return (cpu.total == 100 + 10 + 50 + 1000 + 200);
	}
}

int main(int argc, char** argv) {
	return test::suite {
		{ "/proc/stat parsing", parseProcStat },
		{ "re-ordered parsing", parseReordered },
		{ "semi-junk parsing", parseSemiJunk },
	}.run();
}
