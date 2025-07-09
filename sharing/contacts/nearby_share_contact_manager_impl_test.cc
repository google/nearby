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

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/test/fake_account_manager.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
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
using ::nearby::sharing::proto::ContactRecord;

constexpr char kTestDefaultDeviceName[] = "Josh's Chromebook";
constexpr char kTestProfileUserName[] = "test@google.com";
constexpr char kTestAccountId[] = "test_account_id";
constexpr char kTestDefaultContactsHash[] = "last_hash";

// From nearby_share_contact_manager_impl.cc.
constexpr absl::Duration kContactDownloadPeriod = absl::Hours(12);

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
    EXPECT_TRUE(fake_context_.last_sequenced_task_runner()->SyncWithTimeout(
        absl::Milliseconds(1000)));
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

}  // namespace
}  // namespace sharing
}  // namespace nearby
