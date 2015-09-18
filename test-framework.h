// Copyright (C) 2015 Philip Cronje. All rights reserved.
#pragma once

#include <forward_list>
#include <functional>
#include <iostream>
#include <string>

namespace test {
	struct test {
		const std::string name;
		const std::function<bool()>& test;
	};

	class suite {
		const std::forward_list<test> tests_;
		const unsigned int testCount_;

		public:
			suite(const std::initializer_list<test>& il) : tests_(il), testCount_(il.size()) {}
			int run() const {
				std::cout << "1.." << testCount_ << std::endl;
				int i = 1;
				int exitCode = 0;
				for(const test& test: tests_) {
					const bool result = test.test();
					if(!result) exitCode = 1;
					std::cout << (result ? "ok " : "not ok ") << i++ << " - " << test.name << std::endl;
				}
				return exitCode;
			}
	};
}
