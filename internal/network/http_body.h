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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_BODY_H_
#define THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_BODY_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace nearby {
namespace network {

class HttpBody {
 public:
  HttpBody() = default;
  ~HttpBody() = default;

  HttpBody(const HttpBody&) = default;
  HttpBody& operator=(const HttpBody&) = default;
  HttpBody(HttpBody&&) = default;
  HttpBody& operator=(HttpBody&&) = default;

  void SetData(const char* data, size_t size) {
    if (data == nullptr) {
      size = 0;
    }
    data_.assign(data, size);
  }

  void SetData(absl::string_view data) {
    data_.assign(data.data(), data.size());
  }

  bool empty() { return data_.empty(); }

  const char* data() const { return &data_[0]; }

  size_t size() const { return data_.size(); }

  absl::string_view GetRawData() const { return data_; }

 private:
  std::string data_;
};

using HttpRequestBody = HttpBody;
using HttpResponseBody = HttpBody;

}  // namespace network
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_NETWORK_HTTP_BODY_H_
