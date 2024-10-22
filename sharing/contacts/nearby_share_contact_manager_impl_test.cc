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

#include "sharing/contacts/nearby_share_contact_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/test/fake_account_manager.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
#include "sharing/contacts/nearby_share_contacts_sorter.h"
#include "sharing/internal/api/fake_nearby_share_client.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "sharing/proto/contact_rpc.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/scheduling/fake_nearby_share_scheduler.h"
#include "sharing/scheduling/fake_nearby_share_scheduler_factory.h"
#include "sharing/scheduling/nearby_share_scheduler_factory.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::api::PreferenceManager;
using ::nearby::sharing::proto::Contact;
using ::nearby::sharing::proto::ContactRecord;

constexpr char kTestContactIdPrefix[] = "id_";
constexpr char kTestContactEmailPrefix[] = "email_";
constexpr char kTestContactPhonePrefix[] = "phone_";
constexpr char kTestDefaultDeviceName[] = "Josh's Chromebook";
constexpr char kTestProfileUserName[] = "test@google.com";
constexpr char kTestAccountId[] = "test_account_id";
constexpr char kTestDefaultContactsHash[] = "last_hash";
const char* kTestPersonNames[] = {"BBB BBB", "CCC CCC", "AAA AAA"};

// From nearby_share_contact_manager_impl.cc.
constexpr absl::Duration kContactUploadPeriod = absl::Hours(24);
constexpr absl::Duration kContactDownloadPeriod = absl::Hours(12);

std::string GetTestContactId(size_t index) {
  return absl::StrCat(kTestContactIdPrefix, index);
}
std::string GetTestContactEmail(size_t index) {
  return absl::StrCat(kTestContactEmailPrefix, index);
}
std::string GetTestContactPhone(size_t index) {
  return absl::StrCat(kTestContactPhonePrefix, index);
}

std::set<std::string> TestContactIds(size_t num_contacts) {
  std::set<std::string> ids;
  for (size_t i = 0; i < num_contacts; ++i) {
    ids.insert(GetTestContactId(i));
  }
  return ids;
}

std::vector<ContactRecord> TestContactRecordList(size_t num_contacts) {
  std::vector<ContactRecord> contact_list;
  for (size_t i = 0; i < num_contacts; ++i) {
    ContactRecord contact;
    contact.set_id(GetTestContactId(i));
    contact.set_image_url("https://www.google.com/");
    contact.set_person_name(kTestPersonNames[i % 3]);
    contact.set_is_reachable(true);
    // only one of these fields should be set...
    switch ((i % 3)) {
      case 0:
        contact.add_identifiers()->set_account_name(GetTestContactEmail(i));
        break;
      case 1:
        contact.add_identifiers()->set_phone_number(GetTestContactPhone(i));
        break;
      case 2:
        contact.add_identifiers()->set_obfuscated_gaia("4938tyah");
        break;
    }
    contact_list.push_back(contact);
  }
  return contact_list;
}

// Converts a list of ContactRecord protos, along with the allowlist, into a
// list of Contact protos. To enable self-sharing across devices, we expect the
// local device to include itself in the contact list as an allowed contact.
// Partially from nearby_share_contact_manager_impl.cc.
std::vector<Contact> BuildContactListToUpload(
    const std::vector<ContactRecord>& contact_records) {
  std::vector<Contact> contacts;
  for (const auto& contact_record : contact_records) {
    for (const auto& identifier : contact_record.identifiers()) {
      Contact contact;
      *contact.mutable_identifier() = identifier;
      contact.set_is_selected(/*is_selected=*/true);
      contacts.push_back(contact);
    }
  }

  // Add self to list of contacts.
  Contact contact;
  contact.mutable_identifier()->set_account_name(kTestProfileUserName);
  contact.set_is_selected(true);
  contacts.push_back(contact);

  return contacts;
}

void VerifyDownloadNotificationContacts(
    const std::vector<ContactRecord>& expected_unordered_contacts,
    const std::vector<ContactRecord>& notification_contacts) {
  EXPECT_EQ(notification_contacts.size(), expected_unordered_contacts.size());

  // Verify that observers receive contacts in sorted order.
  std::vector<ContactRecord> expected_ordered_contacts =
      expected_unordered_contacts;
  SortNearbyShareContactRecords(&expected_ordered_contacts);
  for (size_t i = 0; i < expected_ordered_contacts.size(); ++i) {
    EXPECT_EQ(notification_contacts[i].SerializeAsString(),
              expected_ordered_contacts[i].SerializeAsString());
  }
}

class NearbyShareContactManagerImplTest
    : public ::testing::Test,
      public NearbyShareContactManager::Observer {
 protected:
  struct ContactsDownloadedNotification {
    std::vector<ContactRecord> contacts;
    uint32_t num_unreachable_contacts_filtered_out;
  };
  struct ContactsUploadedNotification {
    bool did_contacts_change_since_last_upload;
  };

  NearbyShareContactManagerImplTest()
      : local_device_data_manager_(kTestDefaultDeviceName) {
    local_device_data_manager_.set_is_sync_mode(true);
  }

  ~NearbyShareContactManagerImplTest() override = default;

  void SetUp() override {
    FakeTaskRunner::ResetPendingTasksCount();
    prefs::RegisterNearbySharingPrefs(preference_manager_);
    NearbyShareSchedulerFactory::SetFactoryForTesting(&scheduler_factory_);
    AccountManager::Account account;
    account.id = kTestAccountId;
    account.email = kTestProfileUserName;
    fake_account_manager_.SetAccount(account);

    manager_ = NearbyShareContactManagerImpl::Factory::Create(
        &fake_context_, preference_manager_,
        fake_account_manager_, &nearby_client_factory_,
        &local_device_data_manager_);

    VerifySchedulerInitialization();
    manager_->AddObserver(this);

    preference_manager().SetString(prefs::kNearbySharingContactUploadHashName,
                                   kTestDefaultContactsHash);
    manager_->Start();
  }

  void TearDown() override {
    manager_->RemoveObserver(this);
    manager_.reset();
    NearbyShareSchedulerFactory::SetFactoryForTesting(nullptr);
    preference_manager().SetString(prefs::kNearbySharingContactUploadHashName,
                                   "");
  }

  void Sync() {
    EXPECT_TRUE(FakeTaskRunner::WaitForRunningTasksWithTimeout(
        absl::Milliseconds(1000)));
  }

  void SetUploadResult(bool success) {
    local_device_data_manager_.SetUploadContactsResult(success);
  }

  void SetDownloadSuccessResult(const std::vector<ContactRecord>& contacts) {
    proto::ListContactPeopleResponse response;
    response.set_next_page_token(nullptr);
    response.mutable_contact_records()->Add(contacts.begin(), contacts.end());
    std::vector<absl::StatusOr<proto::ListContactPeopleResponse>> responses;
    responses.push_back(response);
    client()->SetListContactPeopleResponses(responses);
  }

  void SetDownloadFailureResult() {
    std::vector<absl::StatusOr<proto::ListContactPeopleResponse>> responses;
    responses.push_back(absl::InternalError(""));
    client()->SetListContactPeopleResponses(responses);
  }

  void DownloadContacts(bool download_success, bool expect_upload,
                        bool upload_success,
                        std::optional<std::vector<ContactRecord>> contacts,
                        std::optional<std::vector<ContactRecord>>
                            expected_contacts = std::nullopt) {
    // Track for download contacts.
    size_t num_handled_results =
        download_and_upload_scheduler()->handled_results().size();
    size_t num_download_notifications =
        contacts_downloaded_notifications_.size();
    size_t num_upload_contacts_calls =
        local_device_data_manager_.upload_contacts_calls().size();
    // Invoke upload callback from local device data manager.
    size_t num_upload_notifications = contacts_uploaded_notifications_.size();
    size_t num_download_and_upload_handled_results =
        download_and_upload_scheduler()->handled_results().size();

    manager_->DownloadContacts();
    Sync();
    if (download_success) {
      VerifyDownloadNotificationSent(
          /*initial_num_notifications=*/num_download_notifications,
          expected_contacts.has_value() ? *expected_contacts : *contacts);

      // Verify that contacts start uploading if needed.
      EXPECT_EQ(local_device_data_manager_.upload_contacts_calls().size(),
                num_upload_contacts_calls + (expect_upload ? 1 : 0));

      // Verify that the success result is sent to the download/upload scheduler
      // if a subsequent upload isn't required.
      EXPECT_EQ(download_and_upload_scheduler()->handled_results().size(),
                num_handled_results + 1);
      if (!expect_upload) {
        EXPECT_TRUE(download_and_upload_scheduler()->handled_results().back());
        return;
      }

      // Check on upload.
      FakeNearbyShareLocalDeviceDataManager::UploadContactsCall& call =
          local_device_data_manager_.upload_contacts_calls().back();

      std::vector<Contact> expected_upload_contacts = BuildContactListToUpload(
          expected_contacts.has_value() ? *expected_contacts : *contacts);
      // Ordering doesn't matter. Otherwise, because of internal sorting,
      // comparison would be difficult.
      ASSERT_EQ(expected_upload_contacts.size(), call.contacts.size());
      absl::flat_hash_set<std::string> expected_contacts_set;
      absl::flat_hash_set<std::string> call_contacts_set;
      for (size_t i = 0; i < expected_upload_contacts.size(); ++i) {
        expected_contacts_set.insert(
            expected_upload_contacts[i].SerializeAsString());
        call_contacts_set.insert(call.contacts[i].SerializeAsString());
      }

      // Verify upload notification was sent on success.
      EXPECT_EQ(contacts_uploaded_notifications_.size(),
                num_upload_notifications + (upload_success ? 1 : 0));
      // Verify that the result is sent to download/upload scheduler.
      EXPECT_EQ(download_and_upload_scheduler()->handled_results().size(),
                num_download_and_upload_handled_results + 1);
      EXPECT_EQ(download_and_upload_scheduler()->handled_results().back(),
                upload_success);
    } else {
      EXPECT_EQ(download_and_upload_scheduler()->handled_results().size(),
                num_handled_results + 1);
      EXPECT_FALSE(download_and_upload_scheduler()->handled_results().back());
    }
  }

  PreferenceManager& preference_manager() { return preference_manager_; }

  std::vector<ContactsDownloadedNotification>&
  contacts_downloaded_notifications() {
    return contacts_downloaded_notifications_;
  }

  FakeContext& fake_context() { return fake_context_; }

 private:
  // NearbyShareContactManager::Observer:
  void OnContactsDownloaded(
      const std::vector<ContactRecord>& contacts,
      uint32_t num_unreachable_contacts_filtered_out) override {
    ContactsDownloadedNotification notification;
    notification.contacts = contacts;
    notification.num_unreachable_contacts_filtered_out =
        num_unreachable_contacts_filtered_out;
    contacts_downloaded_notifications_.push_back(notification);
  }
  void OnContactsUploaded(bool did_contacts_change_since_last_upload) override {
    ContactsUploadedNotification notification;
    notification.did_contacts_change_since_last_upload =
        did_contacts_change_since_last_upload;
    contacts_uploaded_notifications_.push_back(notification);
  }

  FakeNearbyShareClient* client() {
    return nearby_client_factory_.instances().back();
  }

  FakeNearbyShareScheduler* download_and_upload_scheduler() {
    return scheduler_factory_.pref_name_to_periodic_instance()
        .at(prefs::kNearbySharingSchedulerContactDownloadAndUploadName)
        .fake_scheduler;
  }

  // Verify scheduler input parameters.
  void VerifySchedulerInitialization() {
    FakeNearbyShareSchedulerFactory::PeriodicInstance
        download_and_upload_scheduler_instance =
            scheduler_factory_.pref_name_to_periodic_instance().at(
                prefs::kNearbySharingSchedulerContactDownloadAndUploadName);
    EXPECT_TRUE(download_and_upload_scheduler_instance.fake_scheduler);
    EXPECT_EQ(download_and_upload_scheduler_instance.request_period,
              kContactDownloadPeriod);
    EXPECT_TRUE(download_and_upload_scheduler_instance.retry_failures);
    EXPECT_TRUE(download_and_upload_scheduler_instance.require_connectivity);
  }

  void TriggerDownloadScheduler() {
    // Fire scheduler and verify downloader creation.
    download_and_upload_scheduler()->InvokeRequestCallback();
  }

  void VerifyDownloadNotificationSent(
      size_t initial_num_notifications,
      const std::vector<ContactRecord>& expected_unordered_contacts) {
    EXPECT_EQ(contacts_downloaded_notifications_.size(),
              initial_num_notifications + 1);

    // Verify notification sent to regular (not mojo) observers.
    VerifyDownloadNotificationContacts(
        expected_unordered_contacts,
        contacts_downloaded_notifications_.back().contacts);
  }

  nearby::FakePreferenceManager preference_manager_;
  FakeAccountManager fake_account_manager_;
  FakeContext fake_context_;
  std::vector<ContactsDownloadedNotification>
      contacts_downloaded_notifications_;
  std::vector<ContactsUploadedNotification> contacts_uploaded_notifications_;
  FakeNearbyShareClientFactory nearby_client_factory_;
  FakeNearbyShareLocalDeviceDataManager local_device_data_manager_;
  std::unique_ptr<FakeAccountManager> account_manager_;
  FakeNearbyShareSchedulerFactory scheduler_factory_;
  std::unique_ptr<NearbyShareContactManager> manager_;
};

TEST_F(NearbyShareContactManagerImplTest, DownloadContacts_WithFirstUpload) {
  std::vector<ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/4u);

  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);
  // Because contacts have never been uploaded, a subsequent upload should be
  // requested, which succeeds.
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records);

  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);
  // When contacts are downloaded again, we detect that contacts have not
  // changed, so no upload should be made
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/false,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records);
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_DetectContactListChanged) {
  std::vector<ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);

  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);
  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records);

  // When contacts are downloaded again, we detect that contacts have changed
  // since the last upload.
  contact_records = TestContactRecordList(/*num_contacts=*/4u);
  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records);
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_HashExpiration) {
  std::vector<ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);

  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);

  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records);

  fake_context().fake_clock()->FastForward(absl::Hours(72));
  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);
  // When contacts are downloaded again, we detect that contacts have not
  // changed, but since the hash expired, we upload again.
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records);
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_HashExpirationUploadFailed) {
  std::vector<ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);
  std::set<std::string> allowlist = TestContactIds(/*num_contacts=*/2u);

  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);

  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records);

  fake_context().fake_clock()->FastForward(absl::Hours(72));
  SetDownloadSuccessResult(contact_records);
  SetUploadResult(false);
  // When contacts are downloaded again, we detect that contacts have not
  // changed, but since the hash expired, we upload again.
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/false,
                   /*contacts=*/contact_records);

  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);
  // When contacts are downloaded again, we detect that contacts have not
  // changed, last upload failed, we upload again.
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records);
}

TEST_F(NearbyShareContactManagerImplTest, DownloadContacts_FailDownload) {
  SetDownloadFailureResult();
  DownloadContacts(/*download_success=*/false, /*expect_upload=*/false,
                   /*upload_success=*/false,
                   /*contacts=*/std::nullopt);
}

TEST_F(NearbyShareContactManagerImplTest, DownloadContacts_RetryFailedUpload) {
  std::vector<ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);

  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);

  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records);

  // When contacts are downloaded again, we detect that contacts have changed
  // since the last upload. Fail this upload.
  contact_records = TestContactRecordList(/*num_contacts=*/4u);
  SetDownloadSuccessResult(contact_records);
  SetUploadResult(false);
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/false,
                   /*contacts=*/contact_records);

  // When contacts are downloaded again, we should continue to indicate that
  // contacts have changed since the last upload, and attempt another upload.
  // (In other words, this tests that the contact-upload hash isn't updated
  // prematurely.)
  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records);
}

TEST_F(NearbyShareContactManagerImplTest,
       DownloadContacts_ListIsFilteredByReachable) {
  std::vector<ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/3u);

  contact_records[0].set_is_reachable(false);

  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);

  // Because contacts have never been uploaded, a subsequent upload is
  // requested, which succeeds.
  std::vector<ContactRecord> expected_contacts = contact_records;
  expected_contacts.erase(expected_contacts.begin());
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records,
                   /*expected_contacts=*/expected_contacts);

  ASSERT_EQ(contacts_downloaded_notifications().size(), 1);
  // We should have 1 filtered contact since we only marked one as unreachable.
  EXPECT_EQ(contacts_downloaded_notifications()
                .front()
                .num_unreachable_contacts_filtered_out,
            1);
  // We should have 2 contacts in the notification since we only marked one as
  // unreachable, out of 3.
  EXPECT_EQ(contacts_downloaded_notifications().front().contacts.size(), 2);
}

TEST_F(NearbyShareContactManagerImplTest, ContactUploadHash) {
  EXPECT_EQ(preference_manager().GetString(
                prefs::kNearbySharingContactUploadHashName, std::string()),
            std::string(kTestDefaultContactsHash));

  std::vector<ContactRecord> contact_records =
      TestContactRecordList(/*num_contacts=*/10u);

  SetDownloadSuccessResult(contact_records);
  SetUploadResult(true);
  DownloadContacts(/*download_success=*/true, /*expect_upload=*/true,
                   /*upload_success=*/true,
                   /*contacts=*/contact_records);

  // Hardcode expected contact upload hash to ensure that hashed value is
  // consistent across process starts. If this test starts to fail, check one
  // of the following:
  //   1. Did the test data change? No worries; just update this hash value.
  //   2. Did the hashing function change? As long as the function is stable
  //      across (most) process starts, then everything is okay; just update
  //      this hash value. A changed hash value will result in an extra
  //      server call, so as long as the value is stable for the most part,
  //      it's okay.
  const char kExpectedHash[] =
      "DEE0B27BDA7B56EEA8F7C9A46313D67170D0707A484A541DE907DD9CACE5935E";
  EXPECT_EQ(kExpectedHash,
            preference_manager().GetString(
                prefs::kNearbySharingContactUploadHashName, std::string()));

  // Try a few different permutations of contacts to ensure that the hash is
  // invariant under ordering.
  std::default_random_engine rng;
  for (size_t i = 0; i < 10u; ++i) {
    // We do not expect an upload because the contacts did not change in any
    // way other than ordering.
    std::vector<ContactRecord> shuffled_contacts = contact_records;
    std::shuffle(shuffled_contacts.begin(), shuffled_contacts.end(), rng);

    SetDownloadSuccessResult(shuffled_contacts);
    SetUploadResult(true);
    DownloadContacts(/*download_success=*/true, /*expect_upload=*/false,
                     /*upload_success=*/true,
                     /*contacts=*/shuffled_contacts);

    EXPECT_EQ(preference_manager().GetString(
                  prefs::kNearbySharingContactUploadHashName, std::string()),
              kExpectedHash);
  }
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
