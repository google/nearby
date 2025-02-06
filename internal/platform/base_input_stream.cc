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

#include "internal/platform/base_input_stream.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {

ExceptionOr<ByteArray> BaseInputStream::Read(std::int64_t size) {
  if (!IsAvailable(size)) {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }

  ByteArray read_bytes{static_cast<size_t>(size)};
  if (read_bytes.CopyAt(/*offset=*/0, buffer_,
                        /*source_offset=*/position_)) {
    position_ += size;
    return ExceptionOr<ByteArray>{read_bytes};
  } else {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }
}

std::optional<std::uint8_t> BaseInputStream::ReadUint8() {
  constexpr int byte_size = sizeof(std::uint8_t);
  std::optional<ByteArray> read_bytes = ReadBytes(byte_size);
  if (!read_bytes.has_value()) {
    return std::nullopt;
  }

  const char *data = read_bytes->data();
  return static_cast<std::uint8_t>(data[0]);
}

std::optional<std::int8_t> BaseInputStream::ReadInt8() {
  constexpr int byte_size = sizeof(std::int8_t);
  std::optional<ByteArray> read_bytes = ReadBytes(byte_size);
  if (!read_bytes.has_value()) {
    return std::nullopt;
  }

  const char *data = read_bytes->data();
  return static_cast<std::int8_t>(data[0]);
}

std::optional<std::uint16_t> BaseInputStream::ReadUint16() {
  constexpr int byte_size = sizeof(std::uint16_t);
  std::optional<ByteArray> read_bytes = ReadBytes(byte_size);
  if (!read_bytes.has_value()) {
    return std::nullopt;
  }

  // Convert from network order.
  const unsigned char *data =
      reinterpret_cast<const unsigned char *>(read_bytes->data());
  return static_cast<uint16_t>(data[0] << 8 | data[1]);
}

std::optional<std::int16_t> BaseInputStream::ReadInt16() {
  constexpr int byte_size = sizeof(std::int16_t);
  std::optional<ByteArray> read_bytes = ReadBytes(byte_size);
  if (!read_bytes.has_value()) {
    return std::nullopt;
  }

  // Convert from network order.
  const unsigned char *data =
      reinterpret_cast<const unsigned char *>(read_bytes->data());
  return static_cast<int16_t>(data[0] << 8 | data[1]);
}

std::optional<std::uint32_t> BaseInputStream::ReadUint32() {
  constexpr int byte_size = sizeof(std::uint32_t);
  std::optional<ByteArray> read_bytes = ReadBytes(byte_size);
  if (!read_bytes.has_value()) {
    return std::nullopt;
  }

  // Convert from network order.
  const unsigned char *data =
      reinterpret_cast<const unsigned char *>(read_bytes->data());
  return static_cast<uint32_t>(data[0] << 24 | data[1] << 16 | data[2] << 8 |
                               data[3]);
}

std::optional<std::int32_t> BaseInputStream::ReadInt32() {
  constexpr int byte_size = sizeof(std::uint32_t);
  std::optional<ByteArray> read_bytes = ReadBytes(byte_size);
  if (!read_bytes.has_value()) {
    return std::nullopt;
  }

  // Convert from network order.
  const unsigned char *data =
      reinterpret_cast<const unsigned char *>(read_bytes->data());
  return static_cast<int32_t>(data[0] << 24 | data[1] << 16 | data[2] << 8 |
                              data[3]);
}

std::optional<std::uint64_t> BaseInputStream::ReadUint64() {
  constexpr int byte_size = sizeof(std::uint64_t);
  std::optional<ByteArray> read_bytes = ReadBytes(byte_size);
  if (!read_bytes.has_value()) {
    return std::nullopt;
  }

  // Convert from network order.
  const unsigned char *data =
      reinterpret_cast<const unsigned char *>(read_bytes->data());
  return static_cast<uint64_t>(data[0]) << 56 |
         static_cast<uint64_t>(data[1]) << 48 |
         static_cast<uint64_t>(data[2]) << 40 |
         static_cast<uint64_t>(data[3]) << 32 |
         static_cast<uint64_t>(data[4]) << 24 |
         static_cast<uint64_t>(data[5]) << 16 |
         static_cast<uint64_t>(data[6]) << 8 | static_cast<uint64_t>(data[7]);
}

std::optional<std::int64_t> BaseInputStream::ReadInt64() {
  constexpr int byte_size = sizeof(std::int64_t);
  std::optional<ByteArray> read_bytes = ReadBytes(byte_size);
  if (!read_bytes.has_value()) {
    return std::nullopt;
  }

  // Convert from network order.
  const unsigned char *data =
      reinterpret_cast<const unsigned char *>(read_bytes->data());
  return static_cast<int64_t>(data[0]) << 56 |
         static_cast<int64_t>(data[1]) << 48 |
         static_cast<int64_t>(data[2]) << 40 |
         static_cast<int64_t>(data[3]) << 32 |
         static_cast<int64_t>(data[4]) << 24 |
         static_cast<int64_t>(data[5]) << 16 |
         static_cast<int64_t>(data[6]) << 8 | static_cast<int64_t>(data[7]);
}

std::optional<ByteArray> BaseInputStream::ReadBytes(int size) {
  ExceptionOr<ByteArray> read_bytes_result = Read(size);
  if (!read_bytes_result.ok()) {
    return std::nullopt;
  }

  return read_bytes_result.result();
}

}  // namespace nearby
