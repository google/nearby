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

#include "presence/presence_device.h"

#include <string>
#include <variant>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/platform/ble_connection_info.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/metadata.pb.h"
#include "presence/data_element.h"
#include "presence/presence_action.h"

namespace nearby {
namespace presence {
namespace {

using ::nearby::internal::Metadata;
using ::testing::Contains;

constexpr DeviceMotion::MotionType kDefaultMotionType =
    DeviceMotion::MotionType::kPointAndHold;
constexpr float kDefaultConfidence = 0;
constexpr float kTestConfidence = 0.1;
constexpr absl::string_view kMacAddr = "\x4C\x8B\x1D\xCE\xBA\xD1";
constexpr int kDataElementType = DataElement::kBatteryFieldType;
constexpr absl::string_view kDataElementValue = "15";
constexpr int kTestAction = 3;

Metadata CreateTestMetadata() {
  Metadata metadata;
  metadata.set_account_name("test_account");
  metadata.set_device_name("NP test device");
  metadata.set_device_profile_url("test_image.test.com");
  metadata.set_bluetooth_mac_address(kMacAddr);
  metadata.set_device_type(internal::DEVICE_TYPE_LAPTOP);
  return metadata;
}

TEST(PresenceDeviceTest, DefaultMotionEquals) {
  Metadata metadata = CreateTestMetadata();
  PresenceDevice device1(metadata);
  PresenceDevice device2(metadata);
  EXPECT_EQ(device1, device2);
}

TEST(PresenceDeviceTest, ExplicitInitEquals) {
  Metadata metadata = CreateTestMetadata();
  internal::SharedCredential shared_credential;
  shared_credential.set_credential_type(internal::CREDENTIAL_TYPE_GAIA);
  PresenceDevice device1 =
      PresenceDevice({kDefaultMotionType, kTestConfidence}, metadata,
                     internal::IDENTITY_TYPE_PUBLIC);
  device1.SetDecryptSharedCredential(shared_credential);
  PresenceDevice device2 =
      PresenceDevice({kDefaultMotionType, kTestConfidence}, metadata,
                     internal::IDENTITY_TYPE_PUBLIC);
  device2.SetDecryptSharedCredential(shared_credential);
  EXPECT_EQ(device1, device2);
}

TEST(PresenceDeviceTest, ExplicitInitNotEquals) {
  Metadata metadata = CreateTestMetadata();
  PresenceDevice device1 = PresenceDevice({kDefaultMotionType}, metadata,
                                          internal::IDENTITY_TYPE_PUBLIC);
  PresenceDevice device2 =
      PresenceDevice({kDefaultMotionType, kTestConfidence}, metadata,
                     internal::IDENTITY_TYPE_PRIVATE);
  EXPECT_NE(device1, device2);
}

TEST(PresenceDeviceTest, TestGetBleConnectionInfo) {
  Metadata metadata = CreateTestMetadata();
  PresenceDevice device = PresenceDevice({kDefaultMotionType}, metadata);
  device.AddAction(PresenceAction(kTestAction));
  auto info = (device.GetConnectionInfos().at(0));
  ASSERT_TRUE(std::holds_alternative<nearby::BleConnectionInfo>(info));
  auto ble_info = std::get<nearby::BleConnectionInfo>(info);
  EXPECT_EQ(ble_info.GetMacAddress(),
            kMacAddr);
  EXPECT_EQ(ble_info.GetActions(), std::vector<uint8_t>{kTestAction});
}

TEST(PresenceDeviceTest, TestGetAddExtendedProperties) {
  Metadata metadata = CreateTestMetadata();
  PresenceDevice device = PresenceDevice({kDefaultMotionType}, metadata);
  device.AddExtendedProperty({kDataElementType, kDataElementValue});
  ASSERT_EQ(device.GetExtendedProperties().size(), 1);
  EXPECT_EQ(device.GetExtendedProperties()[0],
            DataElement(kDataElementType, kDataElementValue));
}

TEST(PresenceDeviceTest, TestGetAddExtendedPropertiesVector) {
  Metadata metadata = CreateTestMetadata();
  PresenceDevice device = PresenceDevice({kDefaultMotionType}, metadata);
  device.AddExtendedProperties(
      {DataElement(kDataElementType, kDataElementValue)});
  ASSERT_EQ(device.GetExtendedProperties().size(), 1);
  EXPECT_EQ(device.GetExtendedProperties()[0],
            DataElement(kDataElementType, kDataElementValue));
}

TEST(PresenceDeviceTest, TestAddGetActions) {
  Metadata metadata = CreateTestMetadata();
  PresenceDevice device = PresenceDevice({kDefaultMotionType}, metadata);
  device.AddAction({kTestAction});
  ASSERT_EQ(device.GetActions().size(), 1);
  EXPECT_EQ(device.GetActions()[0], PresenceAction(kTestAction));
}

TEST(PresenceDeviceTest, TestEndpointIdIsCorrectLength) {
  Metadata metadata = CreateTestMetadata();
  PresenceDevice device = PresenceDevice({kDefaultMotionType}, metadata);
  EXPECT_EQ(device.GetEndpointId().length(), kEndpointIdLength);
}

TEST(PresenceDeviceTest, TestEndpointIdIsRandom) {
  Metadata metadata = CreateTestMetadata();
  PresenceDevice device = PresenceDevice({kDefaultMotionType}, metadata);
  EXPECT_EQ(device.GetEndpointId().length(), kEndpointIdLength);
  EXPECT_NE(device.GetEndpointId(), std::string(kEndpointIdLength, 0));
}

TEST(PresenceDeviceTest, TestGetIdentityType) {
  Metadata metadata = CreateTestMetadata();
  PresenceDevice device =
      PresenceDevice(DeviceMotion(), metadata, internal::IDENTITY_TYPE_PUBLIC);
  EXPECT_EQ(device.GetIdentityType(), internal::IDENTITY_TYPE_PUBLIC);
}

TEST(PresenceDeviceTest, TestGetDecryptSharedCredential) {
  Metadata metadata = CreateTestMetadata();
  PresenceDevice device =
      PresenceDevice(DeviceMotion(), metadata, internal::IDENTITY_TYPE_PUBLIC);
  EXPECT_EQ(device.GetDecryptSharedCredential(), std::nullopt);
  internal::SharedCredential shared_credential;
  shared_credential.set_credential_type(internal::CREDENTIAL_TYPE_GAIA);
  device.SetDecryptSharedCredential(shared_credential);
  EXPECT_EQ(device.GetDecryptSharedCredential()->SerializeAsString(),
            shared_credential.SerializeAsString());
}

TEST(PresenceDeviceTest, TestToProtoBytes) {
  Metadata metadata = CreateTestMetadata();
  PresenceDevice device =
      PresenceDevice(DeviceMotion(), metadata, internal::IDENTITY_TYPE_PUBLIC);
  std::string proto_bytes = device.ToProtoBytes();
  location::nearby::connections::PresenceDevice device_frame;
  ASSERT_TRUE(device_frame.ParseFromString(proto_bytes));
  // Public identity.
  EXPECT_THAT(device_frame.identity_type(), Contains(2));
  EXPECT_EQ(device_frame.endpoint_type(),
            location::nearby::connections::PRESENCE_ENDPOINT);
  EXPECT_EQ(device_frame.endpoint_id(), device.GetEndpointId());
  EXPECT_EQ(device_frame.device_type(),
            location::nearby::connections::PresenceDevice::LAPTOP);
  EXPECT_EQ(device_frame.device_name(), "NP test device");
  EXPECT_EQ(device_frame.device_image_url(), "test_image.test.com");
}

}  // namespace
}  // namespace presence
}  // namespace nearby
