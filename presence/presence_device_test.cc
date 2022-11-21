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

#include "presence/presence_device.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace presence {
namespace {
constexpr DeviceMotion::MotionType kDefaultMotionType =
    DeviceMotion::MotionType::kPointAndHold;
constexpr float kDefaultConfidence = 0;
constexpr float kTestConfidence = 0.1;
TEST(PresenceDeviceTest, DefaultConstructorWorks) {
  PresenceDevice device;
  DeviceMotion device_motion;
  EXPECT_EQ(device.GetDeviceMotion(), device_motion);
}

TEST(PresenceDeviceTest, DefaultEquals) {
  PresenceDevice device1;
  PresenceDevice device2;
  EXPECT_EQ(device1, device2);
}

TEST(PresenceDeviceTest, ExplicitInitEquals) {
  PresenceDevice device1 =
      PresenceDevice({kDefaultMotionType, kTestConfidence});
  PresenceDevice device2 =
      PresenceDevice({kDefaultMotionType, kTestConfidence});
  EXPECT_EQ(device1, device2);
}

TEST(PresenceDeviceTest, ExplicitInitNotEquals) {
  PresenceDevice device1 = PresenceDevice({kDefaultMotionType});
  PresenceDevice device2 =
      PresenceDevice({kDefaultMotionType, kTestConfidence});
  EXPECT_NE(device1, device2);
}

TEST(PresenceDeviceTest, CopyInitEquals) {
  PresenceDevice device1 =
      PresenceDevice({kDefaultMotionType, kTestConfidence});
  PresenceDevice device2 = {device1};
  EXPECT_EQ(device1, device2);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
