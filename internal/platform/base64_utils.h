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

#ifndef PLATFORM_BASE_BASE64_UTILS_H_
#define PLATFORM_BASE_BASE64_UTILS_H_

#include <cstdint>
#include <string>
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {

class Base64Utils {
 public:
  static std::string Encode(const ByteArray& bytes);
  static ByteArray Decode(absl::string_view base64_string);
  static std::int32_t BytesToInt(const ByteArray& bytes);
  static ByteArray IntToBytes(std::int32_t value);
  static ExceptionOr<std::int32_t> ReadInt(InputStream* reader);
  static Exception WriteInt(OutputStream* writer, std::int32_t value);
};

}  // namespace nearby

#endif  // PLATFORM_BASE_BASE64_UTILS_H_
