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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_REQUEST_H_
#define THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_REQUEST_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "internal/network/http_body.h"
#include "internal/network/url.h"

namespace nearby {
namespace network {

enum class HttpRequestMethod {
  kOptions,
  kGet,
  kHead,
  kPost,
  kPut,
  kDelete,
  kTrace,
  kConnect,
  kPatch
};

class HttpRequest {
 public:
  HttpRequest() = default;
  explicit HttpRequest(const Url& url);
  ~HttpRequest() = default;

  HttpRequest(const HttpRequest&) = default;
  HttpRequest& operator=(const HttpRequest&) = default;
  HttpRequest(HttpRequest&&) = default;
  HttpRequest& operator=(HttpRequest&&) = default;

  void SetUrl(const Url& url);
  const Url& GetUrl() const;

  void SetMethod(const HttpRequestMethod& method);
  const HttpRequestMethod& GetMethod() const;
  absl::string_view GetMethodString() const;

  void AddHeader(absl::string_view header, absl::string_view value);
  void RemoveHeader(absl::string_view header);
  const absl::flat_hash_map<std::string, std::vector<std::string>>&
  GetAllHeaders() const;

  void AddQueryParameter(absl::string_view query, absl::string_view value);
  void RemoveQueryParameter(absl::string_view query);
  const Url::QueryParameters& GetAllQueryParameters() const;

  void SetBody(const HttpRequestBody& body);
  void SetBody(absl::string_view body);
  const HttpRequestBody& GetBody() const;

 private:
  // The url of the request
  Url url_;

  // The request method: GET, POST, etc.
  HttpRequestMethod method_ = HttpRequestMethod::kGet;

  // The request headers, may include repeat keys
  absl::flat_hash_map<std::string, std::vector<std::string>> headers_;

  // The request body, it may be empty.
  HttpRequestBody body_;
};

}  // namespace network
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_REQUEST_H_
