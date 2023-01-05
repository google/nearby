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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_RESPONSE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_RESPONSE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/network/http_body.h"
#include "internal/network/http_request.h"
#include "internal/network/http_status_code.h"

namespace nearby {
namespace network {

class HttpResponse {
 public:
  HttpResponse() = default;
  ~HttpResponse() = default;

  HttpResponse(const HttpResponse&) = default;
  HttpResponse& operator=(const HttpResponse&) = default;
  HttpResponse(HttpResponse&&) = default;
  HttpResponse& operator=(HttpResponse&&) = default;

  void SetStatusCode(const HttpStatusCode& status_code);
  HttpStatusCode GetStatusCode() const;

  void SetReasonPhrase(absl::string_view reason_phrase);
  absl::string_view GetReasonPhrase() const;

  void AddHeader(absl::string_view header, absl::string_view value);
  void RemoveHeader(absl::string_view header);
  void SetHeaders(const absl::flat_hash_map<std::string,
                                            std::vector<std::string>>& headers);
  const absl::flat_hash_map<std::string, std::vector<std::string>>&
  GetAllHeaders() const;

  void SetBody(const HttpResponseBody& body);
  void SetBody(absl::string_view body);
  const HttpResponseBody& GetBody() const;

 private:
  // The status code returned from remote server
  HttpStatusCode status_code_ = HttpStatusCode::kHttpOk;

  // Provides a short textual description of the Status Code.
  std::string reason_phrase_;

  // The response headers returned from remove server
  absl::flat_hash_map<std::string, std::vector<std::string>> headers_;

  // The response content from remote server, it may be empty
  HttpResponseBody body_;
};

}  // namespace network
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_RESPONSE_H_
