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
#include "absl/time/time.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/test/fake_account_manager.h"
#include "sharing/contacts/nearby_share_contact_manager.h"
#include "sharing/internal/api/fake_nearby_share_client.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "sharing/proto/contact_rpc.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/scheduling/fake_nearby_share_scheduler_factory.h"
#include "sharing/scheduling/nearby_share_scheduler_factory.h"

namespace nearby::sharing {
namespace {

using ::nearby::sharing::proto::ContactRecord;

constexpr char kTestDefaultDeviceName[] = "Josh's Chromebook";
constexpr char kTestProfileUserName[] = "test@google.com";
constexpr char kTestAccountId[] = "test_account_id";

class NearbyShareContactManagerImplTest
    : public ::testing::Test {
 protected:
  struct ContactsDownloadedNotification {
    std::vector<ContactRecord> contacts;
    uint32_t num_unreachable_contacts_filtered_out;
  };
  struct ContactsUploadedNotification {
    bool did_contacts_change_since_last_upload;
  };

  NearbyShareContactManagerImplTest()
      : local_device_data_manager_(kTestDefaultDeviceName) {}

  ~NearbyShareContactManagerImplTest() override = default;

  void SetUp() override {
    NearbyShareSchedulerFactory::SetFactoryForTesting(&scheduler_factory_);
    AccountManager::Account account;
    account.id = kTestAccountId;
    account.email = kTestProfileUserName;
    fake_account_manager_.SetAccount(account);

    manager_ = NearbyShareContactManagerImpl::Factory::Create(
        &fake_context_, fake_account_manager_, &nearby_client_factory_);
  }

  void TearDown() override {
    manager_.reset();
    NearbyShareSchedulerFactory::SetFactoryForTesting(nullptr);
  }

  void Sync() {
    EXPECT_TRUE(fake_context_.last_sequenced_task_runner()->SyncWithTimeout(
        absl::Milliseconds(1000)));
  }

  std::vector<ContactsDownloadedNotification>&
  contacts_downloaded_notifications() {
    return contacts_downloaded_notifications_;
  }

  FakeContext& fake_context() { return fake_context_; }

 private:
  FakeNearbyShareClient* client() {
    return nearby_client_factory_.instances().back();
  }

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
}  // namespace nearby::sharing
