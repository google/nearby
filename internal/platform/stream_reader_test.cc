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
#include "internal/platform/stream_reader.h"

#include <cstdint>
#include <string>

#include "gtest/gtest.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace {

TEST(StreamReaderTest, ReadBits) {
  std::string data{static_cast<char>(0b01011100)};
  ByteArray byte_array(data);
  StreamReader stream{byte_array};
  EXPECT_EQ(stream.ReadBits(1), 0);
  EXPECT_EQ(stream.ReadBits(2), 2);
  EXPECT_EQ(stream.ReadBits(3), 7);
  EXPECT_FALSE(stream.ReadBits(5).has_value());
  EXPECT_EQ(stream.ReadBits(2), 0);
  EXPECT_FALSE(stream.ReadBits(1).has_value());
}

TEST(StreamReaderTest, ReadBitsExceedsByteBoundary) {
  std::string data = "ab";
  ByteArray byte_array(data);
  StreamReader stream{byte_array};
  EXPECT_FALSE(stream.ReadBits(9).has_value());
  EXPECT_EQ(stream.ReadBits(1), 0);
  EXPECT_FALSE(stream.ReadInt16().has_value());
}

TEST(StreamReaderTest, ReadUintValues) {
  std::string data = "\xff\xf1\x0f\x0e\x01\x02\x01\x01\x01\x02\x03\x04\x05\x06";
  ByteArray byte_array(data);
  StreamReader stream{byte_array};
  EXPECT_EQ(stream.ReadUint16(), 0xfff1);
  EXPECT_EQ(stream.ReadUint32(), 0x0f0e0102);
  EXPECT_EQ(stream.ReadUint64(), 0x0101010203040506);
}

TEST(StreamReaderTest, ReadIntValues) {
  std::string data = "\xff\xf1\x0f\x0e\x01\x02\x01\x01\x01\x02\x03\x04\x05\x06";
  ByteArray byte_array(data);
  StreamReader stream{byte_array};
  EXPECT_EQ(stream.ReadInt16(), static_cast<int16_t>(0xfff1));
  EXPECT_EQ(stream.ReadInt32(), static_cast<int32_t>(0x0f0e0102));
  EXPECT_EQ(stream.ReadInt64(), static_cast<int64_t>(0x0101010203040506));
}

}  // namespace
}  // namespace nearby
