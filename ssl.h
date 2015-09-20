// Copyright (C) 2015 Philip Cronje. All rights reserved.
#pragma once

#include <openssl/ssl.h>

#include "net.h"

namespace ssl {
	struct library {
		library();
		~library();
	};

	class context {
		SSL_CTX* context_;

		public:
			context();
			~context() noexcept { SSL_CTX_free(context_); }

			operator SSL_CTX*() const noexcept { return context_; }

			void setDefaultVerifyPaths() const noexcept { SSL_CTX_set_default_verify_paths(context_); }
	};

	class x509 {
		X509* x509_;

		public:
			x509() noexcept : x509_(nullptr) {}
			x509(X509* x509) noexcept : x509_(x509) {}
			~x509() noexcept { if(x509_ != nullptr) X509_free(x509_); }

			operator bool() const noexcept { return x509_ != nullptr; }

			bool matchSan(const std::string& value) const noexcept;
			bool matchCommonName(const std::string& value) const noexcept;
	};

	class connection {
		const context& context_;
		BIO* internalBio_;
		BIO* networkBio_;
		net::socket& socket_;
		SSL* ssl_;

		public:
			connection(const context& context, net::socket& socket);
			~connection();

			operator SSL*() const noexcept { return ssl_; }

			bool inConnectInit() const noexcept { return SSL_in_connect_init(ssl_); }

			x509 peerCertificate() const noexcept;
			void verifyPeerCertificate() const;

			bool connect();

			void readFromSocket() const;
			void writeToSocket() const;
	};
}
