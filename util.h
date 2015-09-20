// Copyright (C) 2015 Philip Cronje. All rights reserved.
#pragma once

namespace util {
	std::string hexEncode(const uint8_t* data, const size_t size) {
		constexpr const char* hex = "0123456789abcdef";
		std::string result;
		for(unsigned int i = 0; i < size; ++i) {
			const uint8_t byte = data[i];
			const uint8_t high = (byte & 0xF0) >> 4;
			const uint8_t low = byte & 0x0F;
			result.push_back(hex[high]);
			result.push_back(hex[low]);
		}
		return result;
	}
}
