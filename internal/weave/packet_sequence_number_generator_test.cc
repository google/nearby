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

#include "internal/weave/packet_sequence_number_generator.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace weave {
namespace {

TEST(PacketSequenceNumberGeneratorTest, TestNext) {
  PacketSequenceNumberGenerator generator;
  EXPECT_EQ(generator.Next(), 0);
  EXPECT_EQ(generator.Next(), 1);
  EXPECT_EQ(generator.Next(), 2);
  EXPECT_EQ(generator.Next(), 3);
}

TEST(PacketSequenceNumberGeneratorTest, TestNextWrapping) {
  PacketSequenceNumberGenerator generator;
  EXPECT_EQ(generator.Next(), 0);
  EXPECT_EQ(generator.Next(), 1);
  EXPECT_EQ(generator.Next(), 2);
  EXPECT_EQ(generator.Next(), 3);
  EXPECT_EQ(generator.Next(), 4);
  EXPECT_EQ(generator.Next(), 5);
  EXPECT_EQ(generator.Next(), 6);
  EXPECT_EQ(generator.Next(), 7);
  EXPECT_EQ(generator.Next(), 0);
  EXPECT_EQ(generator.Next(), 1);
}

TEST(PacketSequenceNumberGeneratorTest, TestReset) {
  PacketSequenceNumberGenerator generator;
  EXPECT_EQ(generator.Next(), 0);
  EXPECT_EQ(generator.Next(), 1);
  EXPECT_EQ(generator.Next(), 2);
  generator.Reset();
  EXPECT_EQ(generator.Next(), 0);
  EXPECT_EQ(generator.Next(), 1);
  EXPECT_EQ(generator.Next(), 2);
}

}  // namespace
}  // namespace weave
}  // namespace nearby
