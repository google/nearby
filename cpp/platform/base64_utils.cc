// Copyright 2020 Google LLC
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

#include "platform/base64_utils.h"

#include "absl/strings/escaping.h"

namespace location {
namespace nearby {

std::string Base64Utils::encode(ConstPtr<ByteArray> bytes) {
  std::string base64_string;

  if (!bytes.isNull()) {
    absl::WebSafeBase64Escape(bytes->asString(), &base64_string);
  }

  return base64_string;
}

std::string Base64Utils::encode(const ByteArray& bytes) {
  std::string base64_string;
  absl::WebSafeBase64Escape(bytes.asString(), &base64_string);

  return base64_string;
}

std::string Base64Utils::encode(absl::string_view input) {
  std::string base64_string;
  absl::WebSafeBase64Escape(input, &base64_string);

  return base64_string;
}

template<>
Ptr<ByteArray> Base64Utils::decode(absl::string_view base64_string) {
  std::string decoded_string;
  if (!absl::WebSafeBase64Unescape(base64_string, &decoded_string)) {
    return Ptr<ByteArray>();
  }

  return MakePtr(new ByteArray(decoded_string));
}

template<>
ByteArray Base64Utils::decode(absl::string_view base64_string) {
  std::string decoded_string;
  if (!absl::WebSafeBase64Unescape(base64_string, &decoded_string)) {
    return ByteArray();
  }

  return ByteArray(decoded_string);
}

Ptr<ByteArray> Base64Utils::decode(absl::string_view base64_string) {
  return decode<Ptr<ByteArray>>(base64_string);
}

}  // namespace nearby
}  // namespace location
