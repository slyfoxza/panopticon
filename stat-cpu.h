// Copyright (C) 2015 Philip Cronje. All rights reserved.
#pragma once

#include <istream>

namespace stat {
	struct cpu {
		unsigned long long total;
		unsigned long long user, system, ioWait;

		cpu() = default;
		explicit cpu(std::istream&& stream);
	};
}
