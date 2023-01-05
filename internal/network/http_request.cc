// Copyright 2022 Google LLC
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

#include "internal/network/http_request.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace nearby {
namespace network {

HttpRequest::HttpRequest(const Url& url) : url_(url) {
  method_ = HttpRequestMethod::kGet;
}

void HttpRequest::SetUrl(const Url& url) { url_ = url; }

const Url& HttpRequest::GetUrl() const { return url_; }

void HttpRequest::SetMethod(const HttpRequestMethod& method) {
  method_ = method;
}

const HttpRequestMethod& HttpRequest::GetMethod() const { return method_; }

absl::string_view HttpRequest::GetMethodString() const {
  switch (method_) {
    case HttpRequestMethod::kConnect:
      return "CONNECT";
    case HttpRequestMethod::kDelete:
      return "DELETE";
    case HttpRequestMethod::kGet:
      return "GET";
    case HttpRequestMethod::kHead:
      return "HEAD";
    case HttpRequestMethod::kOptions:
      return "OPTIONS";
    case HttpRequestMethod::kPatch:
      return "PATCH";
    case HttpRequestMethod::kPost:
      return "POST";
    case HttpRequestMethod::kPut:
      return "PUT";
    case HttpRequestMethod::kTrace:
      return "TRACE";
  }
}

void HttpRequest::AddHeader(absl::string_view header, absl::string_view value) {
  auto it = headers_.find(header);
  if (it == headers_.end()) {
    headers_.emplace(header, std::vector<std::string>({std::string(value)}));
  } else {
    it->second.push_back(std::string(value));
  }
}

void HttpRequest::RemoveHeader(absl::string_view header) {
  headers_.erase(header);
}

const absl::flat_hash_map<std::string, std::vector<std::string>>&
HttpRequest::GetAllHeaders() const {
  return headers_;
}

void HttpRequest::AddQueryParameter(absl::string_view query,
                                    absl::string_view value) {
  url_.AddQueryParameter(query, value);
}

void HttpRequest::RemoveQueryParameter(absl::string_view query) {
  url_.RemoveQueryParameter(query);
}
const Url::QueryParameters& HttpRequest::GetAllQueryParameters() const {
  return url_.GetAllQueryStrings();
}

void HttpRequest::SetBody(const HttpRequestBody& body) { body_ = body; }

void HttpRequest::SetBody(absl::string_view body) {
  body_.SetData(body.data(), body.size());
}

const HttpRequestBody& HttpRequest::GetBody() const { return body_; }

}  // namespace network
}  // namespace nearby
