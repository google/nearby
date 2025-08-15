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
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/test/fake_account_manager.h"
#include "internal/test/fake_device_info.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"

namespace nearby::sharing {
namespace {

const char kDefaultDeviceName[] = "$0\'s $1";
const char kFakeDeviceName[] = "My Cool Chromebook";
const char kFakeEmptyDeviceName[] = "";
const char kFakeGivenName[] = "Barack奥巴马";
const char kFakeInvalidDeviceName[] = "\xC0";
const char kFakeTooLongDeviceName[] = "this string is 33 bytes in UTF-8!";
const char kFakeTooLongGivenName[] = "this is a 33-byte string in utf-8";
constexpr char kTestAccountId[] = "test_account_id";
constexpr char kTestProfileUserName[] = "test@google.com";

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

    AccountManager::Account account;
    account.id = kTestAccountId;
    account.email = kTestProfileUserName;
    account.given_name = kFakeGivenName;
    fake_account_manager_.SetAccount(account);
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
        preference_manager_, fake_account_manager_, fake_device_info_);
    manager_->AddObserver(this);
  }

  void DestroyManager() {
    manager_->RemoveObserver(this);
    manager_.reset();
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

 protected:
  nearby::FakePreferenceManager preference_manager_;
  nearby::FakeAccountManager fake_account_manager_;
  nearby::FakeDeviceInfo fake_device_info_;
  std::vector<ObserverNotification> notifications_;
  std::unique_ptr<NearbyShareLocalDeviceDataManager> manager_;
};

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

}  // namespace
}  // namespace nearby::sharing
