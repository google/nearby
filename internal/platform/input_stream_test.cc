// Copyright 2023 Google LLC
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

#include "internal/platform/input_stream.h"

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;

class TestInputStream : public InputStream {
 public:
  MOCK_METHOD(ExceptionOr<ByteArray>, Read, (std::int64_t), (override));
  MOCK_METHOD(Exception, Close, (), (override));
};

// Returns a ByteArray with values: a, a + 1, a + 2, ..., b - 1.
ExceptionOr<ByteArray> Range(char a, char b) {
  std::string s;
  for (char c = a; c < b; c++) {
    s.push_back(c);
  }
  return ExceptionOr<ByteArray>(ByteArray(s));
}

TEST(InputStreamTest, Skip) {
  NiceMock<TestInputStream> stream;
  InSequence seq;
  EXPECT_CALL(stream, Read(50)).WillOnce(Return(Range(0, 10)));
  EXPECT_CALL(stream, Read(40)).WillOnce(Return(Range(0, 10)));
  EXPECT_CALL(stream, Read(30)).WillOnce(Return(Range(0, 30)));

  ExceptionOr<std::size_t> skipped = stream.Skip(50);

  EXPECT_EQ(skipped.result(), 50);
}

TEST(InputStreamTest, SkipsLessOnEof) {
  NiceMock<TestInputStream> stream;
  InSequence seq;
  EXPECT_CALL(stream, Read(50)).WillOnce(Return(Range(0, 10)));
  EXPECT_CALL(stream, Read(40)).WillOnce(Return(Range(0, 10)));
  // Returns EOF
  EXPECT_CALL(stream, Read(30)).WillOnce(Return(Range(0, 0)));

  ExceptionOr<std::size_t> skipped = stream.Skip(50);

  EXPECT_EQ(skipped.result(), 20);
}

TEST(InputStreamTest, SkipFailsOnError) {
  NiceMock<TestInputStream> stream;
  InSequence seq;
  EXPECT_CALL(stream, Read(50)).WillOnce(Return(Range(0, 10)));
  EXPECT_CALL(stream, Read(40))
      .WillOnce(Return(ExceptionOr<ByteArray>(Exception::kIo)));

  ExceptionOr<std::size_t> skipped = stream.Skip(50);

  EXPECT_EQ(skipped.exception(), Exception::kIo);
}

TEST(InputStreamTest, ReadExactlyOneChunk) {
  NiceMock<TestInputStream> stream;
  EXPECT_CALL(stream, Read(10)).WillOnce(Return(Range(0, 10)));

  ExceptionOr<ByteArray> result = stream.ReadExactly(10);

  EXPECT_EQ(result, Range(0, 10));
}

TEST(InputStreamTest, ReadExactlyMultipleChunks) {
  NiceMock<TestInputStream> stream;
  InSequence seq;
  EXPECT_CALL(stream, Read(30)).WillOnce(Return(Range(0, 10)));
  EXPECT_CALL(stream, Read(20)).WillOnce(Return(Range(10, 20)));
  EXPECT_CALL(stream, Read(10)).WillOnce(Return(Range(20, 30)));

  ExceptionOr<ByteArray> result = stream.ReadExactly(30);

  EXPECT_EQ(result, Range(0, 30));
}

TEST(InputStreamTest, ReadExactlyFailsOnError) {
  NiceMock<TestInputStream> stream;
  InSequence seq;
  EXPECT_CALL(stream, Read(30)).WillOnce(Return(Range(0, 10)));
  EXPECT_CALL(stream, Read(20))
      .WillOnce(Return(ExceptionOr<ByteArray>(Exception::kIo)));

  ExceptionOr<ByteArray> result = stream.ReadExactly(30);

  EXPECT_EQ(result.exception(), Exception::kIo);
}

}  // namespace
}  // namespace nearby
