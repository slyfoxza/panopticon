// Copyright (C) 2015 Philip Cronje. All rights reserved.
#pragma once

#include <memory>

namespace util {
	class buffer {
		std::unique_ptr<uint8_t[]> data_;
		uint8_t* cursor_;
		int size_;

		public:
			buffer() : data_(nullptr), cursor_(nullptr), size_(0) {}
			buffer(std::unique_ptr<uint8_t[]>&& data, const int size) : data_(std::move(data)), cursor_(data_.get()), size_(size) {}
			buffer(util::buffer&& buffer) = default;

			buffer& operator=(buffer&& buffer) = default;

			operator bool() const noexcept { return cursor_ - data_.get() < size_; }
			operator void*() const noexcept { return cursor_; }

			size_t size() const noexcept { return size_; }
			size_t remaining() const noexcept { return size_ - (cursor_ - data_.get()); }

			void advance(const size_t n) {
				if((cursor_ += n) > data_.get() + size_) {
					throw std::logic_error("Overflowed buffer");
				}
			}
	};

	std::string hexEncode(const uint8_t* data, const size_t size);
}
