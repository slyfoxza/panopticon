// Copyright (C) 2015 Philip Cronje. All rights reserved.
#include <iostream> // XXX Debugging

#include <sys/socket.h>

#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "ssl.h"

namespace {
	struct opensslCategory_ : public std::error_category {
		const char* name() const noexcept override { return "openssl"; }
		std::string message(int code) const noexcept override { return ERR_error_string(code, nullptr); }
	};
	opensslCategory_ opensslCategory;

	struct x509Category_ : public std::error_category {
		const char* name() const noexcept override { return "x509"; }
		std::string message(int code) const noexcept override { return X509_verify_cert_error_string(code); }
	};
	x509Category_ x509Category;
}

ssl::library::library() {
	SSL_library_init();
	SSL_load_error_strings();
}

ssl::library::~library() {
	FIPS_mode_set(0);
	ENGINE_cleanup();
	CONF_modules_unload(1);
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_remove_state(0);
	ERR_free_strings();
}

ssl::context::context() : context_(SSL_CTX_new(SSLv23_client_method())) {
	if(context_ == nullptr) throw std::system_error(ERR_get_error(), opensslCategory);
}

bool ssl::x509::matchSan(const std::string& value) const noexcept {
	/*
	const int extensionCount = X509_get_ext_count(x509_);
	for(int i = 0; i < extensionCount; ++i) {
		X509_EXTENSION* extension = X509_get_ext(x509_, i);
	}
	*/
	/*
	const int index = X509_get_ext_by_NID(x509_, NID_subject_alt_name, -1);
	if(index < 0) return false;
	X509_EXTENSION* extension = X509_get_ext(x509_, index);
	ASN1_OBJECT* sanObject = X509_EXTENSION_get_object(extension);
	*/
	GENERAL_NAMES* generalNames = static_cast<GENERAL_NAMES*>(X509_get_ext_d2i(x509_, NID_subject_alt_name, nullptr,
				nullptr));
	if(generalNames == nullptr) return false;
	const int generalNameCount = sk_GENERAL_NAME_num(generalNames);
	for(int i = 0; i < generalNameCount; ++i) {
		GENERAL_NAME* generalName = sk_GENERAL_NAME_value(generalNames, i);
		if(generalName->type != GEN_DNS) continue;
		ASN1_STRING* dnsName = generalName->d.dNSName;
		if((dnsName->data == nullptr) || (dnsName->length == 0)) continue;

		int dnsNameLength;
		char* dnsNameString;
		dnsNameLength = ASN1_STRING_to_UTF8(reinterpret_cast<unsigned char**>(&dnsNameString), dnsName);
		std::cout << "SAN: <" << dnsNameString << "> vs <" << value << ">" << std::endl;
		const bool match = value == dnsNameString;
		OPENSSL_free(dnsNameString);
		if(match) {
			GENERAL_NAMES_free(generalNames);
			return true;
		}
	}
	GENERAL_NAMES_free(generalNames);
	return false;
}

bool ssl::x509::matchCommonName(const std::string& value) const noexcept {
	X509_NAME* subject = X509_get_subject_name(x509_);
	if(subject == nullptr) return false;
	char commonName[256];
	X509_NAME_get_text_by_NID(subject, NID_commonName, commonName, sizeof(commonName));
	commonName[sizeof(commonName) - 1] = '\0';
	return value == commonName;
}

ssl::connection::connection(const context& context, net::socket& socket) : context_(context), socket_(socket),
		ssl_(SSL_new(context_)) {
	const int rc = BIO_new_bio_pair(&internalBio_, 0, &networkBio_, 0);
	if(rc == 0) {
		throw std::system_error(SSL_get_error(ssl_, rc), opensslCategory, "Failed to create TLS BIO pair");
	}
	SSL_set_bio(ssl_, internalBio_, internalBio_);
		}

ssl::connection::~connection() {
	SSL_free(ssl_);
	BIO_free(networkBio_);
}

ssl::x509 ssl::connection::peerCertificate() const noexcept {
	X509* x509 = SSL_get_peer_certificate(ssl_);
	return (x509 == nullptr) ? ssl::x509() : ssl::x509(x509);
}

void ssl::connection::verifyPeerCertificate() const {
	const long rc = SSL_get_verify_result(ssl_);
	if(rc != X509_V_OK) {
		throw std::system_error(rc, x509Category);
	}
}

bool ssl::connection::connect() {
	const int rc = SSL_connect(ssl_);
	if(rc <= 0) {
		const int error = SSL_get_error(ssl_, rc);
		if((error == SSL_ERROR_WANT_READ) || (error == SSL_ERROR_WANT_WRITE)) {
			return false;
		} else {
			throw std::system_error(error, opensslCategory, "TLS handshaking failed");
		}
	}
	return true;
}

void ssl::connection::readFromSocket() const {
	constexpr size_t bufferSize = 4096;
	uint8_t buffer[bufferSize];
	const ssize_t n = recv(socket_, buffer, bufferSize, 0);
	if(n == -1) {
		if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			socket_.readable(false);
			return;
		} else {
			throw std::system_error(errno, std::system_category());
		}
	} else if(n == 0) {
		return;
	}

	const int rc = BIO_write(networkBio_, buffer, n);
	if(rc == -2) {
		throw std::runtime_error("BIO_write not implemented for network BIO");
	} else if(rc <= 0) {
		throw std::system_error(SSL_get_error(ssl_, rc), opensslCategory, "Failed to read from socket into BIO");
	} else if(rc != n) {
		throw std::logic_error("Buffer underflow when reading from socket into BIO");
	}
	std::cout << "Read " << n << " bytes from socket into BIO" << std::endl;
}

void ssl::connection::writeToSocket() const {
	if(BIO_ctrl_pending(networkBio_) <= 0) {
		return;
	}

	constexpr size_t bufferSize = 4096;
	uint8_t buffer[bufferSize];
	const int rc = BIO_read(networkBio_, buffer, bufferSize);
	if(rc == -2) {
		throw std::runtime_error("BIO_read not implemented for network BIO");
	} else if(rc <= 0) {
		throw std::system_error(SSL_get_error(ssl_, rc), opensslCategory, "Failed to write to socket from BIO");
	}

	const ssize_t n = send(socket_, buffer, rc, 0);
	if(n == -1) {
		if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
			socket_.writable(false);
			throw std::logic_error("Discarding data because socket would block");
			return;
		} else {
			throw std::system_error(errno, std::system_category());
		}
	} else if(n != rc) {
		throw std::logic_error("Buffer underflow when writing to socket from BIO");
	}
	std::cout << "Wrote " << n << " bytes to socket from BIO" << std::endl;
}

void ssl::connection::read() const {
	char buffer[4096];
	const int rc = SSL_read(ssl_, buffer, 4095);
	buffer[4095] = '\0';
	if(rc < 0) {
		const int error = SSL_get_error(ssl_, rc);
		if((error == SSL_ERROR_WANT_READ) || (error == SSL_ERROR_WANT_WRITE)) return;
		throw std::system_error(error, opensslCategory, "Failed to read from TLS engine");
	} else {
		buffer[rc] = '\0';
		std::cout << "Read " << rc << " bytes form TLS engine: <" << buffer << '>' << std::endl;
	}
}

void ssl::connection::write(util::buffer& buffer) const {
	const int rc = SSL_write(ssl_, buffer, buffer.remaining());
	if(rc <= 0) {
		const int error = SSL_get_error(ssl_, rc);
		if((error == SSL_ERROR_WANT_READ) || (error == SSL_ERROR_WANT_WRITE)) {
			return;
		}
		throw std::system_error(error, opensslCategory, "Failed to write to TLS engine");
	} else {
		std::cout << "Wrote " << rc << " bytes to TLS engine" << std::endl;
		buffer.advance(rc);
	}
}
