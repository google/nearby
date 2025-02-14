// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_STREAM_WRITER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_STREAM_WRITER_H_

#include <cstdint>
#include <string>

#include "internal/platform/exception.h"

namespace nearby {

// A base {@link StreamWriter } for writing the contents of a byte array.
// All internal data is stored in network order.
class StreamWriter {
 public:
  StreamWriter() = default;
  ~StreamWriter() = default;

  std::string GetData();

  // Writes less than 8 bits to the stream. The written bits cannot across a
  // byte boundary.
  Exception WriteBits(uint8_t value, int bits);
  Exception WriteUint8(uint8_t value);
  Exception WriteInt8(int8_t value);
  Exception WriteUint16(uint16_t value);
  Exception WriteInt16(int16_t value);
  Exception WriteUint32(uint32_t value);
  Exception WriteInt32(int32_t value);
  Exception WriteUint64(uint64_t value);
  Exception WriteInt64(int64_t value);
  Exception WriteBytes(const std::string& data);

 private:
  uint8_t bits_used_{0};
  uint8_t bits_buffer_{0};
  std::string buffer_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_STREAM_WRITER_H_
