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

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"

namespace nearby {
namespace presence {
namespace {

using ::testing::status::StatusIs;

constexpr char kDeviceName[] = "Sample Device";
constexpr char kDeviceImageUrl[] =
    "https://lh3.googleusercontent.com/a-/"
    "AFdZucq1YCEyL_d7GHqPweYIfql-HdjSAwbn13tmAd32xdk=s288-p-rw-no";
constexpr PresenceDevice::MotionType kDefaultMotionType =
    PresenceDevice::MotionType::kPointAndHold;
constexpr absl::string_view kMacAddr = "\x4C\x8B\x1D\xCE\xBA\xD1";
constexpr float kDefaultConfidence = 0;
constexpr float kTestConfidence = 0.1;

TEST(PresenceDeviceTest, TestNoInitCall) {
  PresenceDevice pd(kDefaultMotionType, kTestConfidence, kDeviceName);
  EXPECT_THAT(pd.GetEndpointId(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(PresenceDeviceTest, TestHasPresenceId) {
  PresenceDevice pd(kDefaultMotionType, kTestConfidence, kDeviceName);
  // Init has not been called yet.
  EXPECT_EQ(pd.GetPresenceId(), -1);
  EXPECT_OK(pd.InitPresenceDevice());
  EXPECT_NE(pd.GetPresenceId(), -1);
}

TEST(PresenceDeviceTest, TestHasDiscoveryTime) {
    PresenceDevice pd(kDefaultMotionType, kTestConfidence, kDeviceName);
  // Init has not been called yet.
  EXPECT_EQ(pd.GetDiscoveryTimeMillis(), 0);
  EXPECT_OK(pd.InitPresenceDevice());
  EXPECT_NE(pd.GetDiscoveryTimeMillis(), 0);
}

TEST(PresenceDeviceTest, TestRandomEndpointId) {
  PresenceDevice pd1(kDefaultMotionType, kTestConfidence, kDeviceName),
      pd2(kDefaultMotionType, kTestConfidence, kDeviceName);
  EXPECT_OK(pd1.InitPresenceDevice());
  EXPECT_OK(pd2.InitPresenceDevice());
  EXPECT_NE(pd1.GetEndpointId().value(), pd2.GetEndpointId().value());
}

TEST(PresenceDeviceTest, TestBadEndpointId) {
  PresenceDevice pd(kDefaultMotionType, kTestConfidence, kDeviceName, "ABCDEFG",
                    "", PresenceDevice::PresenceDeviceType::kDisplay,
                    kDeviceImageUrl,
                    1662051266736 /* September 1, 2022 at 12:54pm EST */,
                    ByteArray(std::string(kMacAddr)));
  EXPECT_OK(pd.InitPresenceDevice());
  EXPECT_NE(pd.GetEndpointId().value(), "ABCDEFG");
}

TEST(PresenceDeviceTest, TestCustomDeviceName) {
  PresenceDevice pd(kDefaultMotionType, kTestConfidence, kDeviceName);
  EXPECT_OK(pd.InitPresenceDevice());
  EXPECT_EQ(pd.GetDeviceName().value(), kDeviceName);
}

TEST(PresenceDeviceTest, TestSetDeviceType) {
  PresenceDevice pd(kDefaultMotionType, kTestConfidence, kDeviceName);
  EXPECT_OK(pd.InitPresenceDevice());
  pd.SetPresenceDeviceType(PresenceDevice::PresenceDeviceType::kDisplay);
  EXPECT_EQ(pd.GetPresenceDeviceType().value(),
            PresenceDevice::PresenceDeviceType::kDisplay);
}

TEST(PresenceDeviceTest, TestDeviceImageUrl) {
  PresenceDevice pd(kDefaultMotionType, kTestConfidence, kDeviceName);
  EXPECT_OK(pd.InitPresenceDevice());
  pd.SetDeviceProfileUrl(kDeviceImageUrl);
  EXPECT_EQ(pd.GetDeviceProfileUrl(), kDeviceImageUrl);
}

TEST(PresenceDeviceTest, TestPresenceType) {
  PresenceDevice pd(kDefaultMotionType, kTestConfidence, kDeviceName);
  EXPECT_OK(pd.InitPresenceDevice());
  EXPECT_EQ(pd.GetType(), NearbyDevice::Type::kPresenceDevice);
}

TEST(PresenceDeviceTest, TestFullConstructor) {
  PresenceDevice pd(kDefaultMotionType, kTestConfidence, kDeviceName, "ABCD",
                    "", PresenceDevice::PresenceDeviceType::kDisplay,
                    kDeviceImageUrl,
                    1662051266736 /* September 1, 2022 at 12:54pm EST */,
                    ByteArray(std::string(kMacAddr)));
  EXPECT_OK(pd.InitPresenceDevice());
  EXPECT_EQ(pd.GetEndpointId().value().size(), kEndpointIdLength);
  EXPECT_EQ(pd.GetEndpointId().value(), "ABCD");
  EXPECT_EQ(pd.GetDeviceName().value(), kDeviceName);
  EXPECT_EQ(pd.GetDeviceProfileUrl(), kDeviceImageUrl);
  EXPECT_EQ(pd.GetConnectionInfos().size(), 1);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
