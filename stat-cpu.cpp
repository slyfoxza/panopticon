// Copyright (C) 2015 Philip Cronje. All rights reserved.
#include <cstring>
#include <numeric>
#include <vector>

#include "stat-cpu.h"

namespace {
	enum index {
		User = 0,
		UserNice = 1,
		System = 2,
		IoWait = 4
	};

	void skipLine(std::istream& stream) { while(!(stream.eof() || stream.bad()) && (stream.get() != '\n')); }
}

stat::cpu::cpu(std::istream&& stream) {
	while(!(stream.eof() || stream.bad())) {
		static_assert(sizeof(char) == 1, "Assumption failed: sizeof(char) is not 1");
		char cpuId[4];
		stream.read(cpuId, sizeof(cpuId));
		// Find the line containing the combined CPU counters
		if(std::strncmp(cpuId, "cpu ", sizeof(cpuId)) != 0) {
			skipLine(stream);
			continue;
		}

		stream.exceptions(std::istream::badbit);
		// Read all available values remaining in the line
		std::vector<unsigned long long> values;
		while(stream.good()) {
			unsigned long long value;
			stream >> value;
			if(stream.good()) values.push_back(value);
			if(stream.peek() == '\n') break;
		}

		total = std::accumulate(values.cbegin(), values.cend(), 0ULL);
		user = values[User] + values[UserNice];
		system = values[System];
		ioWait = values[IoWait];
		break;
	}
}
