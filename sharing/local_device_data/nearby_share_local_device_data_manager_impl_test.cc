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

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/test/fake_account_manager.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "proto/identity/v1/resources.pb.h"
#include "proto/identity/v1/rpcs.pb.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/internal/api/fake_nearby_share_client.h"
#include "sharing/internal/public/logging.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/device_rpc.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/timestamp.pb.h"
#include "sharing/scheduling/fake_nearby_share_scheduler_factory.h"
#include "sharing/scheduling/nearby_share_scheduler_factory.h"

namespace nearby {
namespace sharing {
namespace {

using UpdateDeviceResponse = nearby::sharing::proto::UpdateDeviceResponse;
using google::nearby::identity::v1::PublishDeviceResponse;
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
constexpr absl::string_view kTestDeviceId = "1234567890";

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
    EXPECT_EQ(response.ok(), returned_success);
    if (!response.ok()) {
      return;
    }
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

  FakeNearbyIdentityClient* identity_client() {
    return nearby_client_factory_.identity_instances().back();
  }

  void SetDeviceId(absl::string_view id) {
    preference_manager_.SetString(prefs::kNearbySharingDeviceIdName, id);
  }

  void Sync() {
    EXPECT_TRUE(context_.last_sequenced_task_runner()->SyncWithTimeout(
        absl::Milliseconds(1000)));
  }

 protected:
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
  SetDeviceId(kTestDeviceId);
  // A 10-character alphanumeric ID is automatically generated if one doesn't
  // already exist.
  std::string id = manager()->GetId();
  EXPECT_EQ(id, kTestDeviceId);

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
  SetDeviceId(kTestDeviceId);
  UploadContacts(CreateResponse(kFakeFullName, kFakeIconUrl, kFakeIconToken));
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadContacts_Failure) {
  CreateManager();
  SetDeviceId(kTestDeviceId);
  UploadContacts(/*response=*/absl::InternalError(""));
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadCertificates_Success) {
  CreateManager();
  SetDeviceId(kTestDeviceId);
  UploadCertificates(
      CreateResponse(kFakeFullName, kFakeIconUrl, kFakeIconToken));
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, UploadCertificates_Failure) {
  CreateManager();
  SetDeviceId(kTestDeviceId);
  UploadCertificates(/*response=*/absl::InternalError(""));
}

std::vector<nearby::sharing::proto::PublicCertificate> GetTestCertificates() {
  nearby::sharing::proto::PublicCertificate cert1;
  cert1.set_secret_id("id1");
  cert1.set_for_self_share(true);
  cert1.mutable_end_time()->set_seconds(1000);
  cert1.mutable_end_time()->set_nanos(2000);

  nearby::sharing::proto::PublicCertificate cert3;
  cert3.set_secret_id("id3");
  cert3.set_for_self_share(true);
  cert3.mutable_end_time()->set_seconds(3000);
  cert3.mutable_end_time()->set_nanos(300);

  nearby::sharing::proto::PublicCertificate cert2;
  cert2.set_secret_id("id2");
  cert2.set_for_self_share(false);
  cert2.mutable_end_time()->set_seconds(2000);
  cert2.mutable_end_time()->set_nanos(200);

  nearby::sharing::proto::PublicCertificate cert4;
  cert4.set_secret_id("id4");
  cert4.set_for_self_share(false);
  cert4.mutable_end_time()->set_seconds(4000);
  cert4.mutable_end_time()->set_nanos(400);

  nearby::sharing::proto::PublicCertificate cert5;
  cert5.set_secret_id("id5");
  cert5.set_for_self_share(false);
  cert5.set_for_selected_contacts(true);
  cert5.mutable_end_time()->set_seconds(2500);
  cert5.mutable_end_time()->set_nanos(250);

  nearby::sharing::proto::PublicCertificate cert6;
  cert6.set_secret_id("id6");
  cert6.set_for_self_share(false);
  cert6.set_for_selected_contacts(true);
  cert6.mutable_end_time()->set_seconds(4500);
  cert6.mutable_end_time()->set_nanos(450);
  return {cert1, cert2, cert3, cert4, cert5, cert6};
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest,
       PublishDeviceInitialCall_ContactUpdateAdded) {
  CreateManager();
  SetDeviceId(kTestDeviceId);
  bool returned_success;
  bool returned_make_another_call;
  PublishDeviceResponse response;
  response.add_contact_updates(google::nearby::identity::v1::
                                   PublishDeviceResponse::CONTACT_UPDATE_ADDED);

  identity_client()->SetPublishDeviceResponse(
      absl::StatusOr<PublishDeviceResponse>(response));
  manager()->PublishDevice(GetTestCertificates(), /*is_second_call=*/false,
                           [&returned_success, &returned_make_another_call](
                               bool success, bool make_another_call) {
                             returned_success = success;
                             returned_make_another_call = make_another_call;
                           });

  Sync();
  auto request = identity_client()->publish_device_requests().back();
  EXPECT_EQ(request.device().name(),
            absl::StrCat("devices/", manager()->GetId()));
  EXPECT_EQ(request.device().display_name(), "Barack奥巴马's PC");
  EXPECT_EQ(
      request.device().contact(),
      google::nearby::identity::v1::Device::CONTACT_GOOGLE_CONTACT_LATEST);
  ASSERT_EQ(request.device().per_visibility_shared_credentials_size(), 2);

  auto self_credential = request.device().per_visibility_shared_credentials(0);
  EXPECT_EQ(self_credential.visibility(),
            google::nearby::identity::v1::PerVisibilitySharedCredentials::
                VISIBILITY_SELF);
  EXPECT_EQ(self_credential.shared_credentials_size(), 2);
  EXPECT_EQ(self_credential.shared_credentials(0).id(), 4993322223562966528);

  ASSERT_EQ(GetTestCertificates().size(), 6);
  EXPECT_EQ(self_credential.shared_credentials(0).data(),
            GetTestCertificates().at(0).SerializeAsString());
  EXPECT_EQ(self_credential.shared_credentials(0).data_type(),
            google::nearby::identity::v1::SharedCredential::
                DATA_TYPE_PUBLIC_CERTIFICATE);
  EXPECT_EQ(self_credential.shared_credentials(0).expiration_time().seconds(),
            1000);
  EXPECT_EQ(self_credential.shared_credentials(0).expiration_time().nanos(),
            2000);

  EXPECT_EQ(self_credential.shared_credentials(1).id(), 2903692628687846585);
  EXPECT_EQ(self_credential.shared_credentials(1).data(),
            GetTestCertificates().at(2).SerializeAsString());
  EXPECT_EQ(self_credential.shared_credentials(1).data_type(),
            google::nearby::identity::v1::SharedCredential::
                DATA_TYPE_PUBLIC_CERTIFICATE);
  EXPECT_EQ(self_credential.shared_credentials(1).expiration_time().seconds(),
            3000);
  EXPECT_EQ(self_credential.shared_credentials(1).expiration_time().nanos(),
            300);

  auto contact_credential =
      request.device().per_visibility_shared_credentials(1);
  EXPECT_EQ(contact_credential.visibility(),
            google::nearby::identity::v1::PerVisibilitySharedCredentials::
                VISIBILITY_CONTACTS);
  ASSERT_EQ(contact_credential.shared_credentials_size(), 2);
  EXPECT_EQ(contact_credential.shared_credentials(0).id(),
            -5684021477085783942);
  EXPECT_EQ(contact_credential.shared_credentials(0).data(),
            GetTestCertificates().at(1).SerializeAsString());
  EXPECT_EQ(contact_credential.shared_credentials(0).data_type(),
            google::nearby::identity::v1::SharedCredential::
                DATA_TYPE_PUBLIC_CERTIFICATE);
  EXPECT_EQ(
      contact_credential.shared_credentials(0).expiration_time().seconds(),
      2000);
  EXPECT_EQ(contact_credential.shared_credentials(0).expiration_time().nanos(),
            200);

  EXPECT_TRUE(returned_success);
  EXPECT_FALSE(returned_make_another_call);
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest,
       PublishDeviceInitialCall_ContactUpdateRemoved) {
  CreateManager();
  SetDeviceId(kTestDeviceId);
  bool returned_success;
  bool returned_make_another_call;
  PublishDeviceResponse response;
  response.add_contact_updates(
      google::nearby::identity::v1::PublishDeviceResponse::
          CONTACT_UPDATE_REMOVED);

  identity_client()->SetPublishDeviceResponse(
      absl::StatusOr<PublishDeviceResponse>(response));
  manager()->PublishDevice(GetTestCertificates(), /*is_second_call=*/false,
                           [&returned_success, &returned_make_another_call](
                               bool success, bool make_another_call) {
                             returned_success = success;
                             returned_make_another_call = make_another_call;
                           });

  Sync();

  EXPECT_TRUE(returned_success);
  // 2nd call is needed to regenerate all Private certificates.
  EXPECT_TRUE(returned_make_another_call);
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest,
       PublishDeviceSecondCall_ContactUnchanged) {
  CreateManager();
  SetDeviceId(kTestDeviceId);
  bool returned_success;
  bool returned_make_another_call;
  PublishDeviceResponse response;

  identity_client()->SetPublishDeviceResponse(
      absl::StatusOr<PublishDeviceResponse>(response));
  manager()->PublishDevice(GetTestCertificates(), /*is_second_call=*/true,
                           [&returned_success, &returned_make_another_call](
                               bool success, bool make_another_call) {
                             returned_success = success;
                             returned_make_another_call = make_another_call;
                           });

  Sync();
  auto request = identity_client()->publish_device_requests().back();

  EXPECT_EQ(request.device().contact(),
            google::nearby::identity::v1::Device::CONTACT_GOOGLE_CONTACT);

  EXPECT_TRUE(returned_success);
  // Contacts are not changed, no need to make another call.
  EXPECT_FALSE(returned_make_another_call);
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest, PublishDevice_Failure) {
  CreateManager();
  SetDeviceId(kTestDeviceId);
  bool returned_success;
  bool returned_make_another_call;
  identity_client()->SetPublishDeviceResponse(absl::InternalError(""));
  manager()->PublishDevice(GetTestCertificates(), /*is_second_call=*/false,
                           [&returned_success, &returned_make_another_call](
                               bool success, bool make_another_call) {
                             returned_success = success;
                             returned_make_another_call = make_another_call;
                           });

  Sync();
  EXPECT_FALSE(returned_success);
  EXPECT_FALSE(returned_make_another_call);
}

TEST_F(NearbyShareLocalDeviceDataManagerImplTest,
       PublishDevice_FailsWithEmptyDeviceId) {
  CreateManager();
  bool returned_success;
  PublishDeviceResponse response;

  identity_client()->SetPublishDeviceResponse(
      absl::StatusOr<PublishDeviceResponse>(response));
  manager()->PublishDevice(GetTestCertificates(), /*is_second_call=*/true,
                           [&returned_success](
                               bool success, bool make_another_call) {
                             returned_success = success;
                           });

  Sync();
  EXPECT_FALSE(returned_success);
  ASSERT_EQ(identity_client()->publish_device_requests().size(), 0);
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
