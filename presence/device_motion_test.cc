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

#include "presence/device_motion.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace presence {
namespace {
static const DeviceMotion::MotionType kDefaultMotionType =
    DeviceMotion::MotionType::kPointAndHold;
static const float kDefaultConfidence = 0;
static const float kConfidenceForTest = 0.1;
TEST(DeviceMotionTest, DefaultConstructorWorks) {
  DeviceMotion motion;
  EXPECT_EQ(motion.GetMotionType(), kDefaultMotionType);
  EXPECT_EQ(motion.GetConfidence(), kDefaultConfidence);
}

TEST(DeviceMotionTest, DefaultEquals) {
  DeviceMotion motion1;
  DeviceMotion motion2;
  EXPECT_EQ(motion1, motion2);
}

TEST(DeviceMotionTest, ExplicitInitEquals) {
  DeviceMotion motion1 = {kDefaultMotionType, kConfidenceForTest};
  DeviceMotion motion2 = {kDefaultMotionType, kConfidenceForTest};
  EXPECT_EQ(motion1, motion2);
  EXPECT_EQ(motion1.GetConfidence(), kConfidenceForTest);
}

TEST(DeviceMotionTest, ExplicitInitNotEquals) {
  DeviceMotion motion1 = {kDefaultMotionType, kConfidenceForTest};
  DeviceMotion motion2 = {kDefaultMotionType};
  EXPECT_NE(motion1, motion2);
}

TEST(DeviceMotionTest, CopyInitEquals) {
  DeviceMotion motion1;
  DeviceMotion motion2 = {motion1};
  EXPECT_EQ(motion1, motion2);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
