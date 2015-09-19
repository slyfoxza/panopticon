// Copyright (C) 2015 Philip Cronje. All rights reserved.
#pragma once

#include <limits>

namespace stat {
	template<typename V, typename T = unsigned int>
	struct aggregation {
		V min = defaultMinValue();
		V max = defaultMaxValue();
		V sum = 0;
		T count = 0;

		aggregation& operator+=(const V value) {
			min = std::min(min, value);
			max = std::max(max, value);
			sum += value;
			++count;
			return *this;
		}

		private:
			typedef std::numeric_limits<V> limits_;
			constexpr V defaultMinValue() const {
				return limits_::has_infinity ? limits_::infinity() : limits_::max();
			}
			constexpr V defaultMaxValue() const {
				return limits_::has_infinity ? -limits_::infinity() : limits_::lowest();
			}
	};

	template<typename T, typename V>
	constexpr T toPercent(const V value, const V total) {
		return static_cast<double>(value) / static_cast<double>(total) * 100.0;
	}
}
