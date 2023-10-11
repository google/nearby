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

#include "presence/presence_device_provider.h"

#include <type_traits>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/proto/metadata.pb.h"
#include "internal/proto/metadata.proto.h"
#include "presence/presence_device.h"

namespace nearby {
namespace presence {
namespace {
using ::nearby::internal::Metadata;

constexpr absl::string_view kMacAddr = "\x4C\x8B\x1D\xCE\xBA\xD1";
constexpr absl::string_view kManagerAppId = "test_app_id";

Metadata CreateTestMetadata() {
  Metadata metadata;
  metadata.set_device_type(internal::DEVICE_TYPE_PHONE);
  metadata.set_account_name("test_account");
  metadata.set_device_name("NP test device");
  metadata.set_user_name("Test user");
  metadata.set_device_profile_url("test_image.test.com");
  metadata.set_bluetooth_mac_address(kMacAddr);
  return metadata;
}

TEST(PresenceDeviceProviderTest, ProviderIsNotTriviallyConstructible) {
  EXPECT_FALSE(std::is_trivially_constructible<PresenceDeviceProvider>::value);
}

TEST(PresenceDeviceProviderTest, DeviceProviderWorks) {
  PresenceDeviceProvider provider(CreateTestMetadata());
  auto device = provider.GetLocalDevice();
  ASSERT_EQ(device->GetType(), NearbyDevice::Type::kPresenceDevice);
  auto presence_device = static_cast<const PresenceDevice*>(device);
  EXPECT_EQ(presence_device->GetMetadata().SerializeAsString(),
            CreateTestMetadata().SerializeAsString());
}

TEST(PresenceDeviceProviderTest, DeviceProviderCanUpdateDevice) {
  PresenceDeviceProvider provider(CreateTestMetadata());
  auto device = provider.GetLocalDevice();
  ASSERT_EQ(device->GetType(), NearbyDevice::Type::kPresenceDevice);
  auto presence_device = static_cast<const PresenceDevice*>(device);
  EXPECT_EQ(presence_device->GetMetadata().SerializeAsString(),
            CreateTestMetadata().SerializeAsString());
  Metadata new_metadata = CreateTestMetadata();
  new_metadata.set_device_name("NP interop device");
  provider.UpdateMetadata(new_metadata);
  EXPECT_EQ(presence_device->GetMetadata().SerializeAsString(),
            new_metadata.SerializeAsString());
}

TEST(PresenceDeviceProviderTest, SetManagerAppId) {
  PresenceDeviceProvider provider(CreateTestMetadata());
  provider.SetManagerAppId(kManagerAppId);
  EXPECT_EQ(provider.GetManagerAppId(), kManagerAppId);
}

}  // namespace
}  // namespace presence
}  // namespace nearby
