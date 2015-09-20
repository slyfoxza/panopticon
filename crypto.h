// Copyright (C) 2015 Philip Cronje. All rights reserved.
#pragma once

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "util.h"

namespace crypto {
	enum class digestType {
		SHA256
	};

	class digestContext {
		EVP_MD_CTX *context_;
		bool needsReinitialization_;
		const digestType type_;

		void initialize() {
			const EVP_MD* md;
			switch(type_) {
				case digestType::SHA256:
					md = EVP_sha256();
					break;
				default:
					throw std::runtime_error("Unsupported message digest type");
			}

			if(EVP_DigestInit_ex(context_, md, nullptr) == 0) {
				throw std::runtime_error("Failed to initialise message digest context");
			}
			needsReinitialization_ = false;
		}

		public:
			digestContext(const digestType type) : context_(EVP_MD_CTX_create()), type_(type) {
				if(context_ == nullptr) {
					throw std::runtime_error("Failed to allocate message digest context");
				}
				initialize();
			}
			~digestContext() { EVP_MD_CTX_destroy(context_); }

			std::string hashString(const std::string& input) {
				if(needsReinitialization_) {
					initialize();
				}

				if(EVP_DigestUpdate(context_, input.c_str(), input.size() * sizeof(char)) == 0) {
					throw std::runtime_error("Failed to hash input");
				}
				uint8_t rawHash[EVP_MAX_MD_SIZE];
				unsigned int rawHashSize;
				if(EVP_DigestFinal_ex(context_, rawHash, &rawHashSize) == 0) {
					throw std::runtime_error("Failed to obtain hash bytes");
				}
				needsReinitialization_ = true;
				return util::hexEncode(rawHash, rawHashSize);
			}
	};

	void hmac(const uint8_t* key, const size_t keySize, const std::string& data, uint8_t* output) {
		if(HMAC(EVP_sha256(), key, keySize, reinterpret_cast<const unsigned char*>(data.c_str()),
					data.size() * sizeof(char), output, nullptr) == nullptr) {
			throw std::runtime_error("Failed to generate HMAC");
		}
	}

	void hmac(const std::string& key, const std::string& data, uint8_t* output) {
		hmac(reinterpret_cast<const uint8_t*>(key.c_str()), key.size() * sizeof(char), data, output);
	}
}
