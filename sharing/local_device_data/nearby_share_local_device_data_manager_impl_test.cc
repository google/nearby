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

#include "sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"

#include <stddef.h>

#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/test/fake_account_manager.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/internal/api/fake_nearby_share_client.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/device_rpc.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/scheduling/fake_nearby_share_scheduler.h"
#include "sharing/scheduling/fake_nearby_share_scheduler_factory.h"
#include "sharing/scheduling/nearby_share_scheduler_factory.h"

namespace nearby {
namespace sharing {
namespace {

using UpdateDeviceResponse = nearby::sharing::proto::UpdateDeviceResponse;
using Contact = nearby::sharing::proto::Contact;

const char kDefaultDeviceName[] = "$0\'s $1";
const char kFakeDeviceName[] = "My Cool Chromebook";
const char kFakeEmptyDeviceName[] = "";
const char kFakeFullName[] = "Barack Obama";
const char kFakeGivenName[] = "Barack奥巴马";
const char kFakeIconUrl[] = "https://www.google.com";
const char kFakeIconToken[] = "token";
const char kFakeInvalidDeviceName[] = "\xC0";
const char kFakeTooLongDeviceName[] = "this string is 33 bytes in UTF-8!";
const char kFakeTooLongGivenName[] = "this is a 33-byte string in utf-8";
constexpr char kTestAccountId[] = "test_account_id";
constexpr char kTestProfileUserName[] = "test@google.com";

absl::StatusOr<UpdateDeviceResponse> CreateResponse(
    const std::optional<std::string>& full_name,
    const std::optional<std::string>& icon_url,
    const std::optional<std::string>& icon_token) {
  absl::StatusOr<UpdateDeviceResponse> result;
  UpdateDeviceResponse response;
  if (full_name) response.set_person_name(*full_name);

  if (icon_url) response.set_image_url(*icon_url);

  if (icon_token) response.set_image_token(*icon_token);

  result = response;
  return result;
}

std::vector<Contact> GetFakeContacts() {
  Contact contact1;
  Contact contact2;
  contact1.mutable_identifier()->set_account_name("account1");
  contact2.mutable_identifier()->set_account_name("account2");
  return {std::move(contact1), std::move(contact2)};
}

std::vector<nearby::sharing::proto::PublicCertificate> GetFakeCertificates() {
  nearby::sharing::proto::PublicCertificate cert1;
  nearby::sharing::proto::PublicCertificate cert2;
  cert1.set_secret_id("id1");
  cert2.set_secret_id("id2");
  return {std::move(cert1), std::move(cert2)};
}

class NearbyShareLocalDeviceDataManagerImplTest
    : public ::testing::Test,
      public NearbyShareLocalDeviceDataManager::Observer {
 protected:
  struct ObserverNotification {
    ObserverNotification(bool did_device_name_change, bool did_full_name_change,
                         bool did_icon_change)
        : did_device_name_change(did_device_name_change),
          did_full_name_change(did_full_name_change),
          did_icon_change(did_icon_change) {}
    ~ObserverNotification() = default;

    bool operator==(const ObserverNotification& other) const {
      return did_device_name_change == other.did_device_name_change &&
             did_full_name_change == other.did_full_name_change &&
             did_icon_change == other.did_icon_change;
    }

    bool did_device_name_change;
    bool did_full_name_change;
    bool did_icon_change;
  };

  NearbyShareLocalDeviceDataManagerImplTest() = default;
  ~NearbyShareLocalDeviceDataManagerImplTest() override = default;

  void SetUp() override {
    prefs::RegisterNearbySharingPrefs(preference_manager_);
    NearbyShareSchedulerFactory::SetFactoryForTesting(&scheduler_factory_);

    AccountManager::Account account;
    account.id = kTestAccountId;
    account.email = kTestProfileUserName;
    account.given_name = kFakeGivenName;
    fake_account_manager_.SetAccount(account);
  }

  void TearDown() override {
    NearbyShareSchedulerFactory::SetFactoryForTesting(nullptr);
  }

  // NearbyShareLocalDeviceDataManager::Observer:
  void OnLocalDeviceDataChanged(bool did_device_name_change,
                                bool did_full_name_change,
                                bool did_icon_change) override {
    notifications_.emplace_back(did_device_name_change, did_full_name_change,
                                did_icon_change);
  }

  void CreateManager() {
    manager_ = NearbyShareLocalDeviceDataManagerImpl::Factory::Create(
        &context_, preference_manager_, fake_account_manager_,
        fake_device_info_, &nearby_client_factory_);
    manager_->AddObserver(this);
    ++num_manager_creations_;
    num_download_device_data_ = 0;
    manager_->Start();
  }

  void DestroyManager() {
    manager_->RemoveObserver(this);
    manager_.reset();
  }

  void UploadContacts(const absl::StatusOr<UpdateDeviceResponse>& response) {
    std::optional<bool> returned_success;

    client()->SetUpdateDeviceResponse(response);
    manager_->UploadContacts(
        GetFakeContacts(),
        [&returned_success](bool success) { returned_success = success; });
    Sync();
    EXPECT_TRUE(client()->list_public_certificates_requests().empty());
    auto device = client()->update_device_requests().back().device().contacts();
    std::vector<Contact> expected_fake_contacts = GetFakeContacts();
    for (size_t i = 0; i < expected_fake_contacts.size(); ++i) {
      EXPECT_EQ(expected_fake_contacts[i].SerializeAsString(),
                client()
                    ->update_device_requests()
                    .back()
                    .device()
                    .contacts()
                    .at(i)
                    .SerializeAsString());
    }

    EXPECT_EQ(response.ok(), returned_success);
  }

  void UploadCertificates(
      const absl::StatusOr<UpdateDeviceResponse>& response) {
    std::optional<bool> returned_success;
    EXPECT_TRUE(client()->list_contact_people_requests().empty());

    client()->SetUpdateDeviceResponse(response);
    manager_->UploadCertificates(
        GetFakeCertificates(),
        [&returned_success](bool success) { returned_success = success; });

    Sync();
    std::vector<nearby::sharing::proto::PublicCertificate>
        expected_fake_certificates = GetFakeCertificates();
    for (size_t i = 0; i < expected_fake_certificates.size(); ++i) {
      EXPECT_EQ(expected_fake_certificates[i].SerializeAsString(),
                client()
                    ->update_device_requests()
                    .back()
                    .device()
                    .public_certificates()
                    .at(i)
                    .SerializeAsString());
    }

    EXPECT_EQ(response.ok(), returned_success);
  }

  NearbyShareLocalDeviceDataManager* manager() { return manager_.get(); }

  FakeAccountManager& fake_account_manager() { return fake_account_manager_; }

  const std::vector<ObserverNotification>& notifications() {
    return notifications_;
  }

  std::string GetDeviceName() const {
    return fake_device_info_.GetOsDeviceName();
  }

  std::string GetDeviceTypeName() const {
    return fake_device_info_.GetDeviceTypeName();
  }

  FakeNearbyShareClient* client() {
    return nearby_client_factory_.instances().back();
  }

  void Sync() {
    EXPECT_TRUE(
        context_.fake_task_runner()->SyncWithTimeout(absl::Milliseconds(1000)));
  }

 private:
  nearby::FakePreferenceManager preference_manager_;
  nearby::FakeAccountManager fake_account_manager_;
  nearby::FakeDeviceInfo fake_device_info_;
  nearby::FakeContext context_;
  size_t num_manager_creations_ = 0;
  size_t num_download_device_data_ = 0;
  std::vector<ObserverNotification> notifications_;
  FakeNearbyShareClientFactory nearby_client_factory_;
  FakeNearbyShareSchedulerFactory scheduler_factory_;
  std::unique_ptr<NearbyShareLocalDeviceDataManager> manager_;
};

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, DeviceId) {
  CreateManager();

  // A 10-character alphanumeric ID is automatically generated if one doesn't
  // already exist.
  std::string id = manager()->GetId();
  EXPECT_EQ(id.size(), 10u);
  for (const char c : id) EXPECT_TRUE(std::isalnum(c));

  // The ID is persisted.
  DestroyManager();
  CreateManager();
  EXPECT_EQ(manager()->GetId(), id);
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, DefaultDeviceName) {
  CreateManager();

  AccountManager::Account account = *fake_account_manager().GetCurrentAccount();
  // Clear login account.
  fake_account_manager().SetAccount(std::nullopt);
  EXPECT_EQ(manager()->GetDeviceName(),
            GetDeviceName());

  // Set given name and expect full default device name of the form
  // "<given name>'s <device type>."
  fake_account_manager().SetAccount(account);
  EXPECT_EQ(absl::Substitute(kDefaultDeviceName,
                             kFakeGivenName,
                             GetDeviceTypeName()),
            manager()->GetDeviceName());

  // Make sure that when we use a given name that is very long we truncate
  // correctly.
  account.given_name = kFakeTooLongGivenName;
  fake_account_manager().SetAccount(account);
  EXPECT_EQ(kNearbyShareDeviceNameMaxLength, manager()->GetDeviceName().size());
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, ValidateDeviceName) {
  CreateManager();
  EXPECT_EQ(manager()->ValidateDeviceName(kFakeDeviceName),
            DeviceNameValidationResult::kValid);
  EXPECT_EQ(manager()->ValidateDeviceName(kFakeEmptyDeviceName),
            DeviceNameValidationResult::kErrorEmpty);
  EXPECT_EQ(manager()->ValidateDeviceName(kFakeTooLongDeviceName),
            DeviceNameValidationResult::kErrorTooLong);
  EXPECT_EQ(manager()->ValidateDeviceName(kFakeInvalidDeviceName),
            DeviceNameValidationResult::kErrorNotValidUtf8);
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, SetDeviceName) {
  CreateManager();

  std::string expected_default_device_name =
      absl::Substitute(kDefaultDeviceName, kFakeGivenName, GetDeviceTypeName());
  EXPECT_EQ(manager()->GetDeviceName(), expected_default_device_name);
  EXPECT_TRUE(notifications().empty());

  auto error = manager()->SetDeviceName(kFakeEmptyDeviceName);
  EXPECT_EQ(error, DeviceNameValidationResult::kErrorEmpty);
  EXPECT_EQ(manager()->GetDeviceName(), expected_default_device_name);
  EXPECT_TRUE(notifications().empty());

  error = manager()->SetDeviceName(kFakeTooLongDeviceName);
  EXPECT_EQ(error, DeviceNameValidationResult::kErrorTooLong);
  EXPECT_EQ(manager()->GetDeviceName(), expected_default_device_name);
  EXPECT_TRUE(notifications().empty());

  error = manager()->SetDeviceName(kFakeInvalidDeviceName);
  EXPECT_EQ(error, DeviceNameValidationResult::kErrorNotValidUtf8);
  EXPECT_EQ(manager()->GetDeviceName(), expected_default_device_name);
  EXPECT_TRUE(notifications().empty());

  error = manager()->SetDeviceName(kFakeDeviceName);
  EXPECT_EQ(error, DeviceNameValidationResult::kValid);
  EXPECT_EQ(manager()->GetDeviceName(), kFakeDeviceName);
  EXPECT_EQ(notifications().size(), 1u);
  EXPECT_EQ(ObserverNotification(/*did_device_name_change=*/true,
                                 /*did_full_name_change=*/false,
                                 /*did_icon_change=*/false),
            notifications().back());

  // Verify that the data is persisted.
  DestroyManager();
  CreateManager();
  EXPECT_EQ(manager()->GetDeviceName(), kFakeDeviceName);
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadContacts_Success) {
  CreateManager();
  UploadContacts(CreateResponse(kFakeFullName, kFakeIconUrl, kFakeIconToken));
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadContacts_Failure) {
  CreateManager();
  UploadContacts(/*response=*/absl::InternalError(""));
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadCertificates_Success) {
  CreateManager();
  UploadCertificates(
      CreateResponse(kFakeFullName, kFakeIconUrl, kFakeIconToken));
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadCertificates_Failure) {
  CreateManager();
  UploadCertificates(/*response=*/absl::InternalError(""));
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
