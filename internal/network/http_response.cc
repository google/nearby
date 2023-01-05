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

#include "internal/network/http_response.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"

namespace nearby {
namespace network {

void HttpResponse::SetStatusCode(const HttpStatusCode& status_code) {
  status_code_ = status_code;
}

HttpStatusCode HttpResponse::GetStatusCode() const { return status_code_; }

void HttpResponse::SetReasonPhrase(absl::string_view reason_phrase) {
  reason_phrase_ = std::string(reason_phrase);
}

absl::string_view HttpResponse::GetReasonPhrase() const {
  return reason_phrase_;
}

void HttpResponse::AddHeader(absl::string_view header,
                             absl::string_view value) {
  auto it = headers_.find(header);
  if (it == headers_.end()) {
    headers_.emplace(header, std::vector<std::string>({std::string(value)}));
  } else {
    it->second.push_back(std::string(value));
  }
}

void HttpResponse::SetHeaders(
    const absl::flat_hash_map<std::string, std::vector<std::string>>& headers) {
  headers_ = headers;
}

const absl::flat_hash_map<std::string, std::vector<std::string>>&
HttpResponse::GetAllHeaders() const {
  return headers_;
}

void HttpResponse::RemoveHeader(absl::string_view header) {
  headers_.erase(header);
}

void HttpResponse::SetBody(const HttpResponseBody& body) { body_ = body; }

void HttpResponse::SetBody(absl::string_view body) {
  body_.SetData(body.data(), body.size());
}

const HttpResponseBody& HttpResponse::GetBody() const { return body_; }

}  // namespace network
}  // namespace nearby
