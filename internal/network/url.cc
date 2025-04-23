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

#include "internal/network/url.h"

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/network/utils.h"

namespace nearby {
namespace network {

absl::StatusOr<Url> Url::Create(absl::string_view url_string) {
  Url url;
  if (!url.SetUrlPath(url_string)) {
    return absl::InvalidArgumentError("bad url.");
  }

  return url;
}

std::string Url::GetUrlPath() const {
  return scheme_ + "://" + host_ + GetPortString() + path_ + GetQueryString() +
         GetFragmentString();
}

absl::string_view Url::GetScheme() const { return scheme_; }

absl::string_view Url::GetHostName() const { return host_; }

absl::string_view Url::GetPath() const { return path_; }

uint16_t Url::GetPort() const { return port_; }

absl::string_view Url::GetFragment() const { return fragment_; }

bool Url::SetUrlPath(absl::string_view url_string) {
  // Refer to: https://www.rfc-editor.org/rfc/rfc3986#page-50
  if (url_string.starts_with("http://")) {
    url_string = url_string.substr(7);
    scheme_ = "http";
  } else if (url_string.starts_with("https://")) {
    url_string = url_string.substr(8);
    scheme_ = "https";
  } else {
    return false;
  }
  absl::string_view authority;
  size_t path_start = url_string.find_first_of("?/#");
  if (path_start != std::string::npos) {
    authority = url_string.substr(0, path_start);
    url_string = url_string.substr(path_start);
  } else {
    authority = url_string;
    url_string = "";
  }
  if (authority.empty()) {
    return false;
  }
  size_t port_start = authority.find(':');
  if (port_start != std::string::npos) {
    host_ = authority.substr(0, port_start);
    int port;
    if (absl::SimpleAtoi(authority.substr(port_start + 1), &port)) {
      port_ = port;
    } else {
      return false;
    }
  } else {
    host_ = authority;
    port_ = scheme_ == "http" ? 80 : 443;
  }
  if (url_string.empty()) {
    return true;
  }
  size_t query_start = url_string.find_first_of('?');
  if (query_start != std::string::npos) {
    path_ = url_string.substr(0, query_start);
    url_string = url_string.substr(query_start + 1);
  } else {
    path_ = url_string;
  }
  absl::string_view query;
  size_t fragment_start = url_string.find_first_of('#');
  if (fragment_start != std::string::npos) {
    query = url_string.substr(0, fragment_start);
    fragment_ = url_string.substr(fragment_start + 1);
  } else {
    query = url_string;
  }
  if (!query.empty()) {
    // split by &
    size_t start = 0;
    size_t end = 0;
    while ((end = query.find('&', start)) != std::string::npos) {
      absl::string_view kv = query.substr(start, end - start);
      start = end + 1;
      size_t eq_pos = kv.find('=');
      if (eq_pos <= 0 || eq_pos == std::string::npos) {
        continue;
      }
      query_parameters_.push_back(
          {UrlDecode(kv.substr(0, eq_pos)), UrlDecode(kv.substr(eq_pos + 1))});
    }

    if (start < query.size()) {
      size_t eq_pos = query.find('=', start);
      if (eq_pos > start && eq_pos < query.size() - 1) {
        query_parameters_.push_back(
            {UrlDecode(query.substr(start, eq_pos - start)),
             UrlDecode(query.substr(eq_pos + 1))});
      }
    }
  }
  if (path_ == "/") {
    path_ = "";
  }
  return true;
}

void Url::AddQueryParameter(absl::string_view query, absl::string_view value) {
  query_parameters_.push_back({absl::StrCat(query), absl::StrCat(value)});
}

void Url::RemoveQueryParameter(absl::string_view query) {
  auto it = query_parameters_.begin();

  while (it != query_parameters_.end()) {
    if (it->first == query) {
      it = query_parameters_.erase(it);
    } else {
      ++it;
    }
  }
}

std::vector<std::string> Url::GetQueryValues(absl::string_view query) const {
  std::vector<std::string> values;
  auto it = query_parameters_.begin();
  while (it != query_parameters_.end()) {
    if (it->first == query) {
      values.push_back(it->second);
    }
    ++it;
  }
  return values;
}

const Url::QueryParameters& Url::GetAllQueryStrings() const {
  return query_parameters_;
}

std::string Url::GetQueryString() const {
  if (query_parameters_.empty()) {
    return "";
  }

  std::string query = "";
  auto it = query_parameters_.begin();
  bool first = true;

  while (it != query_parameters_.end()) {
    if (first) {
      query += "?";
      first = false;
    } else {
      query += "&";
    }

    query += UrlEncode(it->first) + "=" + UrlEncode(it->second);

    it++;
  }

  return query;
}

std::string Url::GetPortString() const {
  if ((scheme_ == "http" && port_ == 80) ||
      (scheme_ == "https" && port_ == 443)) {
    return "";
  }

  return absl::StrCat(":", port_);
}

std::string Url::GetFragmentString() const {
  if (fragment_.empty()) {
    return "";
  }

  return "#" + fragment_;
}

std::ostream& operator<<(std::ostream& os, const Url& url) {
  os << url.GetUrlPath();
  return os;
}

bool operator==(const Url& url1, const Url& url2) {
  if (url1.GetScheme() != url2.GetScheme()) {
    return false;
  }

  if (url1.GetHostName() != url2.GetHostName()) {
    return false;
  }

  if (url1.GetPort() != url2.GetPort()) {
    return false;
  }

  if (url1.GetPath() != url2.GetPath()) {
    return false;
  }

  auto queries1 = url1.GetAllQueryStrings();
  auto queries2 = url2.GetAllQueryStrings();
  if (queries1.size() != queries2.size()) {
    return false;
  }

  std::vector<bool> state;
  state.reserve(queries1.size());
  for (int i = 0; i < queries1.size(); ++i) {
    state.push_back(false);
  }

  for (int i = 0; i < queries1.size(); ++i) {
    bool found = false;
    for (int j = 0; j < queries2.size(); ++j) {
      if (queries1[i].first == queries2[j].first &&
          queries1[i].second == queries2[j].second) {
        if (state[j]) {
          return false;
        }
        state[j] = true;
        found = true;
        break;
      }
    }

    if (!found) {
      return false;
    }
  }

  if (url1.GetFragment() != url2.GetFragment()) {
    return false;
  }

  return true;
}

}  // namespace network
}  // namespace nearby
