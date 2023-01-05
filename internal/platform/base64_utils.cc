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

#include "internal/platform/base64_utils.h"

#include "absl/strings/escaping.h"
#include "internal/platform/byte_array.h"

namespace nearby {

std::string Base64Utils::Encode(const ByteArray& bytes) {
  std::string base64_string;

  absl::WebSafeBase64Escape(std::string(bytes), &base64_string);

  return base64_string;
}

ByteArray Base64Utils::Decode(absl::string_view base64_string) {
  std::string decoded_string;
  if (!absl::WebSafeBase64Unescape(base64_string, &decoded_string)) {
    return ByteArray();
  }

  return ByteArray(decoded_string.data(), decoded_string.size());
}

}  // namespace nearby
