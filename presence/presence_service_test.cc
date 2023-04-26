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

#include "presence/presence_service.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/medium_environment.h"
#include "presence/presence_client.h"

namespace nearby {
namespace presence {
namespace {

using Metadata = ::nearby::internal::Metadata;

class PresenceServiceTest : public testing::Test {
 protected:
  nearby::MediumEnvironment& env_{nearby::MediumEnvironment::Instance()};
};

Metadata CreateTestMetadata(absl::string_view account_name) {
  Metadata metadata;
  metadata.set_account_name(account_name);
  metadata.set_device_name("NP test device");
  metadata.set_device_profile_url("test_image.test.com");
  metadata.set_bluetooth_mac_address("\xFF\xFF\xFF\xFF\xFF\xFF");
  return metadata;
}

TEST_F(PresenceServiceTest, DefaultConstructorWorks) {
  PresenceService presence_service;
}

TEST_F(PresenceServiceTest, StartThenStopScan) {
  env_.Start();
  absl::Status scan_result;
  ScanCallback scan_callback = {
      .start_scan_cb = [&](absl::Status status) { scan_result = status; },
  };
  PresenceService presence_service;
  PresenceClient client = presence_service.CreatePresenceClient();

  absl::StatusOr<ScanSessionId> scan_session = client.StartScan(
      {},
      {
          .start_scan_cb = [&](absl::Status status) { scan_result = status; },
      });
  absl::StatusOr<ScanSessionId> scan_session_with_default_params =
      client.StartScan(ScanRequest(), ScanCallback());

  ASSERT_OK(scan_session);
  ASSERT_OK(scan_session_with_default_params);
  EXPECT_NE(*scan_session, *scan_session_with_default_params);

  client.StopScan(*scan_session);
  client.StopScan(*scan_session_with_default_params);
  env_.Stop();
}

TEST_F(PresenceServiceTest, UpdatingLocalMetadataWorks) {
  PresenceService presence_service;
  presence_service.UpdateLocalDeviceMetadata(CreateTestMetadata("Test account"),
                                             false, "Test app", {}, 3, 1, {});
  EXPECT_EQ(presence_service.GetLocalDeviceMetadata().SerializeAsString(),
            CreateTestMetadata("Test account").SerializeAsString());
}

}  // namespace
}  // namespace presence
}  // namespace nearby
