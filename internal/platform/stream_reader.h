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

#ifndef PLATFORM_BASE_STREAM_READERH_
#define PLATFORM_BASE_STREAM_READERH_

#include <cstddef>
#include <cstdint>
#include <optional>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {

// A base {@link InputStream } for reading the contents of a byte array.
class StreamReader {
 public:
  explicit StreamReader(ByteArray &buffer) : buffer_{buffer} {}
  StreamReader(const StreamReader &) = delete;
  StreamReader &operator=(const StreamReader &) = delete;
  ~StreamReader() = default;

  // Reads less than 8 bits from the stream, returning the value if available.
  // The read bits cannot across a byte boundary.
  std::optional<std::uint8_t> ReadBits(int bits);
  std::optional<std::uint8_t> ReadUint8();
  std::optional<std::int8_t> ReadInt8();
  std::optional<std::uint16_t> ReadUint16();
  std::optional<std::int16_t> ReadInt16();
  std::optional<std::uint32_t> ReadUint32();
  std::optional<std::int32_t> ReadInt32();
  std::optional<std::uint64_t> ReadUint64();
  std::optional<std::int64_t> ReadInt64();
  std::optional<ByteArray> ReadBytes(int size);

  bool IsAvailable(int size) const {
    return buffer_.size() - position_ >= size;
  }

 private:
  ExceptionOr<ByteArray> Read(std::int64_t size);

  uint8_t bits_unused_{0};
  uint8_t bits_buffer_{0};
  ByteArray &buffer_;
  size_t position_{0};
};

}  // namespace nearby

#endif  // PLATFORM_BASE_STREAM_READERH_
