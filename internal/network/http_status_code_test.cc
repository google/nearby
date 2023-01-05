// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/network/http_status_code.h"

#include "gtest/gtest.h"

namespace nearby {
namespace network {
namespace {

TEST(HttpStatusCode, TestReturnCorrectReasonPhrase) {
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpContinue), "Continue");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpSwitchingProtocols),
            "Switching Protocols");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kEarlyHints), "Early Hints");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpOk), "OK");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpCreated), "Created");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpAccepted), "Accepted");
  EXPECT_EQ(
      GetHttpReasonPhrase(HttpStatusCode::kHttpNonAuthoritativeInformation),
      "Non-Authoritative Information");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpNoContent), "No Content");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpResetContent),
            "Reset Content");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpPartialContent),
            "Partial Content");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpMultipleChocies),
            "Multiple Choices");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpMovedPermancently),
            "Moved Permanently");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpFound), "Found");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpSeeOther), "See Other");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpNotModified),
            "Not Modified");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpUseProxy), "Use Proxy");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpTemporaryRedirect),
            "Temporary Redirect");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpPermanentRedirect),
            "Permanent Redirect");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpBadRequest),
            "Bad Request");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpUnauthorized),
            "Unauthorized");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpPaymentRequest),
            "Payment Required");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpForbidden), "Forbidden");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpNotFound), "Not Found");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpMethodNotAllowed),
            "Method Not Allowed");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpNotAcceptable),
            "Not Acceptable");
  EXPECT_EQ(
      GetHttpReasonPhrase(HttpStatusCode::kHttpProxyAuthenticationRequired),
      "Proxy Authentication Required");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpRequestTimeout),
            "Request Timeout");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpConflict), "Conflict");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpGone), "Gone");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpLengthRequired),
            "Length Required");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpPreconditionFailed),
            "Precondition Failed");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpRequestEntityTooLarge),
            "Request Entity Too Large");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpUnsupportedMediaType),
            "Unsupported Media Type");
  EXPECT_EQ(
      GetHttpReasonPhrase(HttpStatusCode::kHttpRequestedRangeNotSatisfiable),
      "Requested Range Not Satisfiable");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpExpectationFailed),
            "Expectation Failed");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpInvalidXprivetToken),
            "Invalid XPrivet Token");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpTooEarly), "Too Early");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpTooManyRequests),
            "Too Many Requests");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpInternalServerError),
            "Internal Server Error");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpNotImplemented),
            "Not Implemented");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpBadGateway),
            "Bad Gateway");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpServiceUnavailable),
            "Service Unavailable");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpGatewayTimeout),
            "Gateway Timeout");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpVersionNotSupported),
            "HTTP Version Not Supported");
  EXPECT_EQ(GetHttpReasonPhrase(HttpStatusCode::kHttpRequestUriTooLong),
            "Request-URI Too Long");
}

}  // namespace
}  // namespace network
}  // namespace nearby
