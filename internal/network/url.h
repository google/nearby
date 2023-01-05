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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_NETWORK_URL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_NETWORK_URL_H_

#include <iostream>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace nearby {
namespace network {

class Url {
 public:
  using QueryParameters = std::vector<std::pair<std::string, std::string>>;

  Url() = default;
  ~Url() = default;

  Url(const Url&) = default;
  Url& operator=(const Url&) = default;
  Url(Url&&) = default;
  Url& operator=(Url&&) = default;

  static absl::StatusOr<Url> Create(absl::string_view url_string);

  bool SetUrlPath(absl::string_view url_path);
  std::string GetUrlPath() const;

  absl::string_view GetScheme() const;
  absl::string_view GetHostName() const;
  absl::string_view GetPath() const;
  uint16_t GetPort() const;
  absl::string_view GetFragment() const;

  void AddQueryParameter(absl::string_view query, absl::string_view value);
  void RemoveQueryParameter(absl::string_view query);
  std::vector<std::string> GetQueryValues(absl::string_view query) const;
  const QueryParameters& GetAllQueryStrings() const;

 private:
  bool ApplyUrlString(absl::string_view url_string);
  std::string GetQueryString() const;
  std::string GetPortString() const;
  std::string GetFragmentString() const;

  // The absolute requested URL encoded in ASCII per the rules of RFC-2396.
  std::string scheme_;
  std::string host_;
  uint16_t port_;
  std::string path_;
  std::string fragment_;

  // Query strings of the request, no URL encode
  QueryParameters query_parameters_;
};

std::ostream& operator<<(std::ostream& os, const Url& url);

bool operator==(const Url& url1, const Url& url2);

}  // namespace network
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_NETWORK_URL_H_
