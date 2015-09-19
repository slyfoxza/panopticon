// Copyright (C) 2015 Philip Cronje. All rights reserved.
#pragma once

#include <istream>

#include "stat.h"

namespace stat {
	struct cpu {
		unsigned long long total;
		unsigned long long user, system, ioWait;

		cpu() = default;
		explicit cpu(std::istream&& stream);

		template<typename V, typename T>
		void aggregate(const cpu& previous, aggregation<V, T>& user, aggregation<V, T>& system,
				aggregation<V, T>& ioWait) {
			const unsigned long long dTotal = total - previous.total;
			user += toPercent<V>(this->user - previous.user, dTotal);
			system += toPercent<V>(this->system - previous.system, dTotal);
			ioWait += toPercent<V>(this->ioWait - previous.ioWait, dTotal);
		}
	};
}
