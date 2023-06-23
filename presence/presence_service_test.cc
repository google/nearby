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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"
#include "presence/presence_client.h"
#include "presence/presence_service_impl.h"

namespace nearby {
namespace presence {
namespace {

using Metadata = ::nearby::internal::Metadata;

constexpr absl::string_view kManagerAppId = "TEST_MANAGER_APP";
constexpr absl::string_view kAccountName = "test account";

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

CredentialSelector BuildDefaultCredentialSelector() {
  CredentialSelector credential_selector;
  credential_selector.manager_app_id = std::string(kManagerAppId);
  credential_selector.account_name = std::string(kAccountName);
  credential_selector.identity_type = internal::IDENTITY_TYPE_PRIVATE;
  return credential_selector;
}

TEST_F(PresenceServiceTest, DefaultConstructorWorks) {
  PresenceServiceImpl presence_service;
}

TEST_F(PresenceServiceTest, StartThenStopScan) {
  env_.Start();
  absl::Status scan_result;
  ScanCallback scan_callback = {
      .start_scan_cb = [&](absl::Status status) { scan_result = status; },
  };
  PresenceServiceImpl presence_service;
  std::unique_ptr<PresenceClient> client =
      presence_service.CreatePresenceClient();

  absl::StatusOr<ScanSessionId> scan_session = client->StartScan(
      {},
      {
          .start_scan_cb = [&](absl::Status status) { scan_result = status; },
      });
  absl::StatusOr<ScanSessionId> scan_session_with_default_params =
      client->StartScan(ScanRequest(), ScanCallback());

  ASSERT_OK(scan_session);
  ASSERT_OK(scan_session_with_default_params);
  EXPECT_NE(*scan_session, *scan_session_with_default_params);

  client->StopScan(*scan_session);
  client->StopScan(*scan_session_with_default_params);
  env_.Stop();
}

TEST_F(PresenceServiceTest, UpdatingLocalMetadataWorks) {
  PresenceServiceImpl presence_service;
  presence_service.UpdateLocalDeviceMetadata(CreateTestMetadata("Test account"),
                                             false, "Test app", {}, 3, 1, {});
  EXPECT_EQ(presence_service.GetLocalDeviceMetadata().SerializeAsString(),
            CreateTestMetadata("Test account").SerializeAsString());
}

TEST_F(PresenceServiceTest, TestGetDeviceProvider) {
  PresenceServiceImpl presence_service;
  EXPECT_NE(presence_service.GetLocalDeviceProvider(), nullptr);
}

TEST_F(PresenceServiceTest, TestGetPublicCredentials) {
  PresenceServiceImpl presence_service;
  CredentialSelector selector = BuildDefaultCredentialSelector();
  absl::Status status;
  nearby::CountDownLatch fetched_latch(1);
  presence_service.GetLocalPublicCredentials(
      selector,
      {.credentials_fetched_cb =
           [&status, &fetched_latch](
               absl::StatusOr<std::vector<nearby::internal::SharedCredential>>
                   result) {
             status = result.status();
             fetched_latch.CountDown();
           }});
  EXPECT_TRUE(fetched_latch.Await().Ok());
  EXPECT_THAT(status, testing::status::StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(PresenceServiceTest, TestUpdateRemotePublicCredentials) {
  PresenceServiceImpl presence_service;
  internal::SharedCredential public_credential_for_test;
  public_credential_for_test.set_identity_type(
      internal::IdentityType::IDENTITY_TYPE_TRUSTED);
  std::vector<internal::SharedCredential> public_credentials{
      {public_credential_for_test}};

  nearby::CountDownLatch updated_latch(1);
  UpdateRemotePublicCredentialsCallback update_credentials_cb{
      .credentials_updated_cb =
          [&updated_latch](absl::Status status) {
            if (status.ok()) {
              updated_latch.CountDown();
            }
          },
  };

  presence_service.UpdateRemotePublicCredentials(
      kManagerAppId, kAccountName, public_credentials,
      std::move(update_credentials_cb));

  EXPECT_TRUE(updated_latch.Await().Ok());
}

}  // namespace
}  // namespace presence
}  // namespace nearby
