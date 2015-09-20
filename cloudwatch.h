// Copyright (C) 2015 Philip Cronje. All rights reserved.
#pragma once

#include <map>
#include <sstream>
#include <string>
#include <unordered_map>

#include "crypto.h"
#include "stat.h"
#include "util.h"

namespace aws {
	namespace cloudwatch {
		void addMetricData(std::ostream& payload, const int index,
				const std::pair<std::string, stat::aggregation<double>>& metricData,
				const std::unordered_map<std::string, std::string>& dimensions) {
			const char* prefix = "&MetricData.member.";
			const char* dimensionPrefix = ".Dimensions.member.";
			const char* statisticValuesPrefix = ".StatisticValues.";

			payload << prefix << index << ".MetricName=" << metricData.first
				<< prefix << index << ".Unit=Percent"
				<< prefix << index << statisticValuesPrefix << "Minimum=" << metricData.second.min
				<< prefix << index << statisticValuesPrefix << "Maximum=" << metricData.second.max
				<< prefix << index << statisticValuesPrefix << "Sum=" << metricData.second.sum
				<< prefix << index << statisticValuesPrefix << "SampleCount=" << metricData.second.count;
			int dimensionIndex = 0;
			for(const auto& dimension: dimensions) {
				++dimensionIndex;
				payload << prefix << index << dimensionPrefix << dimensionIndex << ".Name=" << dimension.first
					<< prefix << index << dimensionPrefix << dimensionIndex << ".Value=" << dimension.second;
			}
		}

		void newPutMetricDataRequest(const std::string& hostName, const std::string& region,
				const std::string& accessKey, const std::string& secretKey,const std::string& nameSpace,
				const std::unordered_map<std::string, std::string>& dimensions,
				const std::unordered_map<std::string, stat::aggregation<double>>& aggregations) {
			const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			const auto nowTm = std::gmtime(&now);

			std::ostringstream payload;
			payload << "Action=PutMetricData&Version=2010-08-01"
				<< "&Namespace=" << nameSpace;

			int metricDataCount = 0;
			for(const auto& metricData: aggregations) {
				addMetricData(payload, ++metricDataCount, metricData, dimensions);
			}

			const std::string payloadString = payload.str();
			crypto::digestContext digestContext(crypto::digestType::SHA256);
			const std::string payloadHash = digestContext.hashString(payloadString);

			char headerDate[30];
			std::strftime(headerDate, sizeof(headerDate), "%a, %d %b %Y %T %Z", nowTm); // TODO: Return value

			std::map<std::string, std::string> headers {
				{ "date", headerDate },
				{ "host", hostName },
			};

			std::ostringstream canonicalString;
			canonicalString << "POST\n" // Request method
				"/\n" // Canonical URI
				"\n"; // Canonical query string
			for(const auto& header: headers) canonicalString << header.first << ':' << header.second << '\n';
			canonicalString << '\n';

			std::ostringstream signedHeaders;
			auto headerIt = headers.cbegin();
			signedHeaders << headerIt->first;
			for(++headerIt; headerIt != headers.cend(); ++headerIt) {
				signedHeaders << ';' << headerIt->first;
			}
			canonicalString << signedHeaders.str() << '\n' << payloadHash << '\n';

			const std::string canonicalRequestHash = digestContext.hashString(canonicalString.str());

			char amazonDate[17];
			std::strftime(amazonDate, sizeof(amazonDate), "%Y%m%dT%H%M%SZ", nowTm); // TODO: Return value
			const std::string amazonDateOnly(amazonDate, amazonDate + 8);

			std::ostringstream credentialScope;
			credentialScope << amazonDateOnly << '/' << region << "/cloudwatch/aws4_request";

			std::ostringstream signingString;
			signingString << "AWS4-HMAC-SHA256\n"
				<< amazonDate << "\n"
				<< credentialScope.str() << '\n'
				<< canonicalRequestHash;

			uint8_t hmac1[256 / 8];
			uint8_t hmac2[256 / 8];
			crypto::hmac("AWS4" + secretKey, amazonDateOnly, hmac1);
			crypto::hmac(hmac1, sizeof(hmac1), region, hmac2);
			crypto::hmac(hmac2, sizeof(hmac2), "cloudwatch", hmac1);
			crypto::hmac(hmac1, sizeof(hmac1), "aws4_request", hmac2);
			std::string signature = util::hexEncode(hmac2, sizeof(hmac2));

			std::ostringstream authorization;
			authorization << "AWS4-HMAC-SHA256 Credential=" << accessKey << '/' << credentialScope.str()
				<< ", SignedHeaders=" << signedHeaders.str() << ", Signature=" << signature;
			headers.emplace("authorization", authorization.str());

			std::ostringstream request;
			request << "POST / HTTP/1.1\r\n";
			for(const auto& header: headers) request << header.first << ": " << header.second << "\r\n";
			request << "\r\n" << payloadString;

			// TODO: Return request as byte buffer
		}
	}
}
