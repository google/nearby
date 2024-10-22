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

#include "presence/presence_client.h"

#include <memory>
#include <optional>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/future.h"
#include "internal/platform/medium_environment.h"
#include "presence/data_types.h"
#include "presence/presence_device.h"
#include "presence/presence_service_impl.h"

namespace nearby {
namespace presence {
namespace {

using ::nearby::internal::DeviceIdentityMetaData;
using ::testing::status::StatusIs;

constexpr absl::string_view kMacAddr = "\x4C\x8B\x1D\xCE\xBA\xD1";

// Creates a PresenceClient and destroys PresenceServiceImpl that was used to
// create it.
std::unique_ptr<PresenceClient> CreateDefunctPresenceClient() {
  PresenceServiceImpl presence_service;
  return presence_service.CreatePresenceClient();
}

DeviceIdentityMetaData CreateTestDeviceIdentityMetaData() {
  DeviceIdentityMetaData device_identity_metadata;
  device_identity_metadata.set_device_type(
      internal::DeviceType::DEVICE_TYPE_PHONE);
  device_identity_metadata.set_device_name("NP test device");
  device_identity_metadata.set_bluetooth_mac_address(kMacAddr);
  device_identity_metadata.set_device_id("\x12\xab\xcd");
  return device_identity_metadata;
}

class PresenceClientTest : public testing::Test {
 protected:
  nearby::MediumEnvironment& env_{nearby::MediumEnvironment::Instance()};
};

TEST_F(PresenceClientTest, StartBroadcastWithDefaultConstructor) {
  env_.Start();
  absl::Status broadcast_result;

  PresenceServiceImpl presence_service;
  std::unique_ptr<PresenceClient> presence_client =
      presence_service.CreatePresenceClient();
  auto unused = presence_client->StartBroadcast(
      {}, {
              .start_broadcast_cb =
                  [&](absl::Status status) { broadcast_result = status; },
          });

  EXPECT_THAT(broadcast_result, StatusIs(absl::StatusCode::kInvalidArgument));
  env_.Stop();
}

TEST_F(PresenceClientTest, StartBroadcastFailsWhenPresenceServiceIsGone) {
  env_.Start();
  absl::Status broadcast_result = absl::UnknownError("");

  absl::StatusOr<BroadcastSessionId> session_id =
      CreateDefunctPresenceClient()->StartBroadcast(
          {}, {
                  .start_broadcast_cb =
                      [&](absl::Status status) { broadcast_result = status; },
              });

  EXPECT_THAT(session_id, StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(broadcast_result, StatusIs(absl::StatusCode::kUnknown));
  env_.Stop();
}

TEST_F(PresenceClientTest, StartScanWithDefaultConstructor) {
  env_.Start();
  ::nearby::Future<absl::Status> scan_result;
  ScanCallback scan_callback = {
      .start_scan_cb = [&](absl::Status status) { scan_result.Set(status); },
  };

  PresenceServiceImpl presence_service;
  std::unique_ptr<PresenceClient> presence_client =
      presence_service.CreatePresenceClient();
  EXPECT_OK(presence_client->StartScan({}, std::move(scan_callback)));

  EXPECT_TRUE(scan_result.Get().ok());
  EXPECT_OK(scan_result.Get().GetResult());
  env_.Stop();
}

TEST_F(PresenceClientTest, StartScanFailsWhenPresenceServiceIsGone) {
  env_.Start();
  absl::Status scan_result = absl::UnknownError("");

  absl::StatusOr<ScanSessionId> session_id =
      CreateDefunctPresenceClient()->StartScan(
          {}, {
                  .start_scan_cb =
                      [&](absl::Status status) { scan_result = status; },
              });

  EXPECT_THAT(session_id, StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(scan_result, StatusIs(absl::StatusCode::kUnknown));
  env_.Stop();
}

TEST_F(PresenceClientTest, GettingDeviceWorks) {
  PresenceServiceImpl presence_service;
  std::unique_ptr<PresenceClient> presence_client =
      presence_service.CreatePresenceClient();
  presence_service.UpdateDeviceIdentityMetaData(
      CreateTestDeviceIdentityMetaData(), false, "", {}, 0, 0, {});
  auto device = presence_client->GetLocalDevice();
  ASSERT_NE(device, std::nullopt);
  EXPECT_EQ(device->GetEndpointId().length(), kEndpointIdLength);
  EXPECT_EQ(device->GetDeviceIdentityMetadata().SerializeAsString(),
            CreateTestDeviceIdentityMetaData().SerializeAsString());
}

TEST_F(PresenceClientTest, TestGettingDeviceDefunct) {
  std::unique_ptr<PresenceClient> presence_client =
      CreateDefunctPresenceClient();
  auto device = presence_client->GetLocalDevice();
  EXPECT_EQ(device, std::nullopt);
}
}  // namespace
}  // namespace presence
}  // namespace nearby
