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

#include <cstdint>
#include <string>
#include <utility>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

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

std::int32_t Base64Utils::BytesToInt(const ByteArray& bytes) {
  const char* int_bytes = bytes.data();

  std::int32_t result = 0;
  result |= (static_cast<std::int32_t>(int_bytes[0]) & 0x0FF) << 24;
  result |= (static_cast<std::int32_t>(int_bytes[1]) & 0x0FF) << 16;
  result |= (static_cast<std::int32_t>(int_bytes[2]) & 0x0FF) << 8;
  result |= (static_cast<std::int32_t>(int_bytes[3]) & 0x0FF);

  return result;
}

ByteArray Base64Utils::IntToBytes(std::int32_t value) {
  char int_bytes[sizeof(std::int32_t)];
  int_bytes[0] = static_cast<char>((value >> 24) & 0x0FF);
  int_bytes[1] = static_cast<char>((value >> 16) & 0x0FF);
  int_bytes[2] = static_cast<char>((value >> 8) & 0x0FF);
  int_bytes[3] = static_cast<char>((value) & 0x0FF);

  return ByteArray(int_bytes, sizeof(int_bytes));
}

ExceptionOr<std::int32_t> Base64Utils::ReadInt(InputStream* reader) {
  ExceptionOr<ByteArray> read_bytes = reader->ReadExactly(sizeof(std::int32_t));
  if (!read_bytes.ok()) {
    return ExceptionOr<std::int32_t>(read_bytes.exception());
  }
  return ExceptionOr<std::int32_t>(
      BytesToInt(std::move(read_bytes.result())));
}

Exception Base64Utils::WriteInt(OutputStream* writer, std::int32_t value) {
  return writer->Write(IntToBytes(value));
}

}  // namespace nearby
