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

#include "internal/platform/stream_writer.h"

#include <cstdint>
#include <string>

#include "internal/platform/exception.h"

namespace nearby {

std::string StreamWriter::GetData() {
  if (bits_used_ != 0) {
    return buffer_ + std::string(1, bits_buffer_);
  }
  return buffer_;
}

Exception StreamWriter::WriteBits(uint8_t value, int bits) {
  if (bits >= 8 || bits <= 0 || 8 - bits < bits_used_) {
    return Exception{Exception::kInvalidProtocolBuffer};
  }

  bits_buffer_ |= (value & ((1 << bits) - 1)) << (8 - bits - bits_used_);
  bits_used_ += bits;

  if (bits_used_ == 8) {
    buffer_.push_back(bits_buffer_);
    bits_used_ = 0;
    bits_buffer_ = 0;
  }

  return Exception{Exception::kSuccess};
}

Exception StreamWriter::WriteUint8(uint8_t value) {
  if (bits_used_ != 0) {
    return Exception{Exception::kInvalidProtocolBuffer};
  }

  buffer_.push_back(value);
  return Exception{Exception::kSuccess};
}

Exception StreamWriter::WriteInt8(int8_t value) {
  return WriteUint8(static_cast<uint8_t>(value));
}

Exception StreamWriter::WriteUint16(uint16_t value) {
  if (bits_used_ != 0) {
    return Exception{Exception::kInvalidProtocolBuffer};
  }

  char bytes[2];
  bytes[0] = (value >> 8) & 0xff;
  bytes[1] = value & 0xff;
  buffer_.append(bytes, 2);
  return Exception{Exception::kSuccess};
}

Exception StreamWriter::WriteInt16(int16_t value) {
  return WriteUint16(static_cast<uint16_t>(value));
}

Exception StreamWriter::WriteUint32(uint32_t value) {
  if (bits_used_ != 0) {
    return Exception{Exception::kInvalidProtocolBuffer};
  }

  char bytes[4];
  bytes[0] = (value >> 24) & 0xff;
  bytes[1] = (value >> 16) & 0xff;
  bytes[2] = (value >> 8) & 0xff;
  bytes[3] = value & 0xff;
  buffer_.append(bytes, 4);
  return Exception{Exception::kSuccess};
}

Exception StreamWriter::WriteInt32(int32_t value) {
  return WriteUint32(static_cast<uint32_t>(value));
}

Exception StreamWriter::WriteUint64(uint64_t value) {
  if (bits_used_ != 0) {
    return Exception{Exception::kInvalidProtocolBuffer};
  }

  char bytes[8];
  bytes[0] = (value >> 56) & 0xff;
  bytes[1] = (value >> 48) & 0xff;
  bytes[2] = (value >> 40) & 0xff;
  bytes[3] = (value >> 32) & 0xff;
  bytes[4] = (value >> 24) & 0xff;
  bytes[5] = (value >> 16) & 0xff;
  bytes[6] = (value >> 8) & 0xff;
  bytes[7] = value & 0xff;
  buffer_.append(bytes, 8);
  return Exception{Exception::kSuccess};
}

Exception StreamWriter::WriteInt64(int64_t value) {
  return WriteUint64(static_cast<uint64_t>(value));
}

Exception StreamWriter::WriteBytes(const std::string& data) {
  if (bits_used_ != 0) {
    return Exception{Exception::kInvalidProtocolBuffer};
  }

  buffer_.append(data);
  return Exception{Exception::kSuccess};
}

}  // namespace nearby
