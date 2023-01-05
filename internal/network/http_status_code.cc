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

namespace nearby {
namespace network {

absl::string_view GetHttpReasonPhrase(HttpStatusCode code) {
  switch (code) {
    case HttpStatusCode::kHttpContinue:
      return "Continue";
    case HttpStatusCode::kHttpSwitchingProtocols:
      return "Switching Protocols";
    case HttpStatusCode::kEarlyHints:
      return "Early Hints";
    case HttpStatusCode::kHttpOk:
      return "OK";
    case HttpStatusCode::kHttpCreated:
      return "Created";
    case HttpStatusCode::kHttpAccepted:
      return "Accepted";
    case HttpStatusCode::kHttpNonAuthoritativeInformation:
      return "Non-Authoritative Information";
    case HttpStatusCode::kHttpNoContent:
      return "No Content";
    case HttpStatusCode::kHttpResetContent:
      return "Reset Content";
    case HttpStatusCode::kHttpPartialContent:
      return "Partial Content";
    case HttpStatusCode::kHttpMultipleChocies:
      return "Multiple Choices";
    case HttpStatusCode::kHttpMovedPermancently:
      return "Moved Permanently";
    case HttpStatusCode::kHttpFound:
      return "Found";
    case HttpStatusCode::kHttpSeeOther:
      return "See Other";
    case HttpStatusCode::kHttpNotModified:
      return "Not Modified";
    case HttpStatusCode::kHttpUseProxy:
      return "Use Proxy";
    case HttpStatusCode::kHttpTemporaryRedirect:
      return "Temporary Redirect";
    case HttpStatusCode::kHttpPermanentRedirect:
      return "Permanent Redirect";
    case HttpStatusCode::kHttpBadRequest:
      return "Bad Request";
    case HttpStatusCode::kHttpUnauthorized:
      return "Unauthorized";
    case HttpStatusCode::kHttpPaymentRequest:
      return "Payment Required";
    case HttpStatusCode::kHttpForbidden:
      return "Forbidden";
    case HttpStatusCode::kHttpNotFound:
      return "Not Found";
    case HttpStatusCode::kHttpMethodNotAllowed:
      return "Method Not Allowed";
    case HttpStatusCode::kHttpNotAcceptable:
      return "Not Acceptable";
    case HttpStatusCode::kHttpProxyAuthenticationRequired:
      return "Proxy Authentication Required";
    case HttpStatusCode::kHttpRequestTimeout:
      return "Request Timeout";
    case HttpStatusCode::kHttpConflict:
      return "Conflict";
    case HttpStatusCode::kHttpGone:
      return "Gone";
    case HttpStatusCode::kHttpLengthRequired:
      return "Length Required";
    case HttpStatusCode::kHttpPreconditionFailed:
      return "Precondition Failed";
    case HttpStatusCode::kHttpRequestEntityTooLarge:
      return "Request Entity Too Large";
    case HttpStatusCode::kHttpRequestUriTooLong:
      return "Request-URI Too Long";
    case HttpStatusCode::kHttpUnsupportedMediaType:
      return "Unsupported Media Type";
    case HttpStatusCode::kHttpRequestedRangeNotSatisfiable:
      return "Requested Range Not Satisfiable";
    case HttpStatusCode::kHttpExpectationFailed:
      return "Expectation Failed";
    case HttpStatusCode::kHttpInvalidXprivetToken:
      return "Invalid XPrivet Token";
    case HttpStatusCode::kHttpTooEarly:
      return "Too Early";
    case HttpStatusCode::kHttpTooManyRequests:
      return "Too Many Requests";
    case HttpStatusCode::kHttpInternalServerError:
      return "Internal Server Error";
    case HttpStatusCode::kHttpNotImplemented:
      return "Not Implemented";
    case HttpStatusCode::kHttpBadGateway:
      return "Bad Gateway";
    case HttpStatusCode::kHttpServiceUnavailable:
      return "Service Unavailable";
    case HttpStatusCode::kHttpGatewayTimeout:
      return "Gateway Timeout";
    case HttpStatusCode::kHttpVersionNotSupported:
      return "HTTP Version Not Supported";
  }
}

}  // namespace network
}  // namespace nearby
