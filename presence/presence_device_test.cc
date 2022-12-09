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

#include <memory>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/types/variant.h"
#include "internal/platform/ble_connection_info.h"
#include "internal/platform/logging.h"
#include "internal/proto/device_metadata.pb.h"

namespace nearby {
namespace presence {
namespace {

using internal::DeviceMetadata;

constexpr DeviceMotion::MotionType kDefaultMotionType =
    DeviceMotion::MotionType::kPointAndHold;
constexpr float kDefaultConfidence = 0;
constexpr float kTestConfidence = 0.1;
constexpr absl::string_view kMacAddr = "\x4C\x8B\x1D\xCE\xBA\xD1";

DeviceMetadata CreateTestDeviceMetadata() {
  DeviceMetadata device_metadata;
  device_metadata.set_stable_device_id("test_device_id");
  device_metadata.set_account_name("test_account");
  device_metadata.set_device_name("NP test device");
  device_metadata.set_icon_url("test_image.test.com");
  device_metadata.set_bluetooth_mac_address(kMacAddr);
  device_metadata.set_device_type(internal::DeviceMetadata::PHONE);
  return device_metadata;
}

TEST(PresenceDeviceTest, DefaultMotionEquals) {
  DeviceMetadata metadata = CreateTestDeviceMetadata();
  PresenceDevice device1(metadata);
  PresenceDevice device2(metadata);
  EXPECT_EQ(device1, device2);
}

TEST(PresenceDeviceTest, ExplicitInitEquals) {
  DeviceMetadata metadata = CreateTestDeviceMetadata();
  PresenceDevice device1 =
      PresenceDevice({kDefaultMotionType, kTestConfidence}, metadata);
  PresenceDevice device2 =
      PresenceDevice({kDefaultMotionType, kTestConfidence}, metadata);
  EXPECT_EQ(device1, device2);
}

TEST(PresenceDeviceTest, ExplicitInitNotEquals) {
  DeviceMetadata metadata = CreateTestDeviceMetadata();
  PresenceDevice device1 = PresenceDevice({kDefaultMotionType}, metadata);
  PresenceDevice device2 =
      PresenceDevice({kDefaultMotionType, kTestConfidence}, metadata);
  EXPECT_NE(device1, device2);
}

TEST(PresenceDeviceTest, TestGetBluetoothAddress) {
  DeviceMetadata metadata = CreateTestDeviceMetadata();
  PresenceDevice device = PresenceDevice({kDefaultMotionType}, metadata);
  auto info = (device.GetConnectionInfos().at(0));
  ASSERT_TRUE(
      absl::holds_alternative<location::nearby::BleConnectionInfo>(info));
  EXPECT_EQ(absl::get<location::nearby::BleConnectionInfo>(info)
                .GetMacAddress()
                .AsStringView(),
            kMacAddr);
}

TEST(PresenceDeviceTest, TestEndpointIdIsCorrectLength) {
  DeviceMetadata metadata = CreateTestDeviceMetadata();
  PresenceDevice device = PresenceDevice({kDefaultMotionType}, metadata);
  EXPECT_EQ(device.GetEndpointId().length(), kEndpointIdLength);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
