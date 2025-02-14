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

#include <string>

#include "gtest/gtest.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace {

TEST(StreamWriterTest, WriteBits) {
  StreamWriter writer;
  EXPECT_EQ(writer.WriteBits(0x01, 1), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteBits(0x01, 1), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteBits(0x01, 1), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteBits(0x00, 1), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.GetData(), std::string("\xe0"));
  EXPECT_EQ(writer.WriteBits(0x01, 1), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteBits(0x01, 1), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteBits(0x01, 1), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteBits(0x01, 1), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.GetData(), std::string("\xef"));
}

TEST(StreamWriterTest, WriteInvalidBits) {
  StreamWriter writer;
  EXPECT_EQ(writer.WriteBits(0x01, 1), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteBits(0x01, 9),
            Exception{Exception::kInvalidProtocolBuffer});
  EXPECT_EQ(writer.WriteBits(0x03, 4), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteBits(0x01, 5),
            Exception{Exception::kInvalidProtocolBuffer});
}

TEST(StreamWriterTest, WriteIntegerValues) {
  StreamWriter writer;
  EXPECT_EQ(writer.WriteUint8(0x01), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteInt8(0xf1), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteUint16(0x0102), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteInt16(0xf102), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.GetData(), "\x01\xF1\x01\x02\xF1\x02");
  EXPECT_EQ(writer.WriteUint32(0x01020304), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteInt32(0xf1020304), Exception{Exception::kSuccess});
  writer = {};
  EXPECT_EQ(writer.WriteUint64(0x0102030405060708),
            Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteInt64(0xf102030405060708),
            Exception{Exception::kSuccess});
  EXPECT_EQ(writer.GetData(),
            "\x01\x02\x03\x04\x05\x06\x07\x08\xF1\x02\x03\x04\x05\x06\x07\x08");
}

TEST(StreamWriterTest, InvalidWriteIntegerValues) {
  StreamWriter writer;
  EXPECT_EQ(writer.WriteBits(0x01, 1), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.WriteUint8(0x01),
            Exception{Exception::kInvalidProtocolBuffer});
  EXPECT_EQ(writer.WriteUint16(0x0102),
            Exception{Exception::kInvalidProtocolBuffer});
  EXPECT_EQ(writer.WriteUint32(0x01020304),
            Exception{Exception::kInvalidProtocolBuffer});
  EXPECT_EQ(writer.WriteUint64(0x0102030405060708),
            Exception{Exception::kInvalidProtocolBuffer});
}

TEST(StreamWriterTest, WriteBytes) {
  StreamWriter writer;
  EXPECT_EQ(writer.WriteBytes("test"), Exception{Exception::kSuccess});
  EXPECT_EQ(writer.GetData(), std::string("test"));
}

}  // namespace
}  // namespace nearby
