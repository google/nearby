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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_STATUS_CODE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_STATUS_CODE_H_

#include "absl/strings/string_view.h"

namespace nearby {
namespace network {

// HTTP response status codes are separated into five categories. Details can be
// found in https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
enum class HttpStatusCode {
  // Informational 1xx: The request has been accepted and needs to be processed
  // further.
  kHttpContinue = 100,
  kHttpSwitchingProtocols = 101,
  kEarlyHints = 103,

  // Information 2xx: The request has been accepted and approved by the server.
  kHttpOk = 200,
  kHttpCreated = 201,
  kHttpAccepted = 202,
  kHttpNonAuthoritativeInformation = 203,
  kHttpNoContent = 204,
  kHttpResetContent = 205,
  kHttpPartialContent = 206,

  // Redirection 3xx: The request needs client's further action to be completed.
  kHttpMultipleChocies = 300,
  kHttpMovedPermancently = 301,
  kHttpFound = 302,
  kHttpSeeOther = 303,
  kHttpNotModified = 304,
  kHttpUseProxy = 305,
  // 306 is no longer used.
  kHttpTemporaryRedirect = 307,
  kHttpPermanentRedirect = 308,

  // Client error 4xx: There may be errors in the client that prevent the server
  // from processing the request.
  kHttpBadRequest = 400,
  kHttpUnauthorized = 401,
  kHttpPaymentRequest = 402,
  kHttpForbidden = 403,
  kHttpNotFound = 404,
  kHttpMethodNotAllowed = 405,
  kHttpNotAcceptable = 406,
  kHttpProxyAuthenticationRequired = 407,
  kHttpRequestTimeout = 408,
  kHttpConflict = 409,
  kHttpGone = 410,
  kHttpLengthRequired = 411,
  kHttpPreconditionFailed = 412,
  kHttpRequestEntityTooLarge = 413,
  kHttpRequestUriTooLong = 414,
  kHttpUnsupportedMediaType = 415,
  kHttpRequestedRangeNotSatisfiable = 416,
  kHttpExpectationFailed = 417,
  // 418 returned by Cloud Print.
  kHttpInvalidXprivetToken = 418,
  kHttpTooEarly = 425,
  kHttpTooManyRequests = 429,

  // Server error 5xx
  kHttpInternalServerError = 500,
  kHttpNotImplemented = 501,
  kHttpBadGateway = 502,
  kHttpServiceUnavailable = 503,
  kHttpGatewayTimeout = 504,
  kHttpVersionNotSupported = 505
};

absl::string_view GetHttpReasonPhrase(HttpStatusCode code);

}  // namespace network
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_STATUS_CODE_H_
