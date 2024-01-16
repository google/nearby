// Copyright 2022-2023 Google LLC
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

#include "sharing/nearby_share_profile_info_provider_impl.h"

#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/test/fake_account_manager.h"
#include "internal/test/fake_device_info.h"

namespace nearby {
namespace sharing {
namespace {

constexpr char kTestAccountId[] = "test_account_id";
constexpr char kTestAccountGivenName[] = "given_name";
constexpr char kExpectedTestAccountGivenName[] = "given_name";
constexpr char kProfileGivenName[] = "Barack";
constexpr char kProfileProfileUserName[] = "test@gmail.com";

}  // namespace

class NearbyShareProfileInfoProviderImplTest : public ::testing::Test {
 protected:
  NearbyShareProfileInfoProviderImplTest() = default;
  ~NearbyShareProfileInfoProviderImplTest() override = default;

  void SetUp() override {
    fake_device_info_.SetGivenName(std::nullopt);
    fake_device_info_.SetProfileUserName(std::nullopt);
  }

  void SetUserGivenName(const std::string& name) {
    fake_device_info_.SetGivenName(name);
  }

  void SetProfileUserName(const std::string& profile_user_name) {
    fake_device_info_.SetProfileUserName(profile_user_name);
  }

  FakeDeviceInfo& fake_device_info() {
    return fake_device_info_;
  }

  FakeAccountManager& fake_account_manager() { return fake_account_manager_; }

 private:
  FakeAccountManager fake_account_manager_;
  FakeDeviceInfo fake_device_info_;
};

TEST_F(NearbyShareProfileInfoProviderImplTest, GivenName) {
  SetProfileUserName(kProfileProfileUserName);
  NearbyShareProfileInfoProviderImpl profile_info_provider(
      fake_device_info(), fake_account_manager());

  // If no user, return std::nullopt.
  EXPECT_FALSE(profile_info_provider.GetGivenName());

  // If given name is empty, return std::nullopt.
  SetUserGivenName(std::string());
  EXPECT_FALSE(profile_info_provider.GetGivenName());

  SetUserGivenName(kProfileGivenName);
  EXPECT_EQ(profile_info_provider.GetGivenName(), kProfileGivenName);
}

TEST_F(NearbyShareProfileInfoProviderImplTest, ProfileUserName) {
  {
    // If profile username is empty, return std::nullopt.
    SetProfileUserName(std::string());
    NearbyShareProfileInfoProviderImpl profile_info_provider(
        fake_device_info(), fake_account_manager());
    EXPECT_FALSE(profile_info_provider.GetProfileUserName());
  }
  {
    SetProfileUserName(kProfileProfileUserName);
    NearbyShareProfileInfoProviderImpl profile_info_provider(
        fake_device_info(), fake_account_manager());
    EXPECT_EQ(profile_info_provider.GetProfileUserName(),
              kProfileProfileUserName);
  }
}

TEST_F(NearbyShareProfileInfoProviderImplTest, GivenNameUseLoginAccount) {
  AccountManager::Account account;
  account.id = kTestAccountId;
  account.given_name = kTestAccountGivenName;
  fake_account_manager().SetAccount(account);
  SetUserGivenName(kProfileGivenName);

  NearbyShareProfileInfoProviderImpl profile_info_provider(
      fake_device_info(), fake_account_manager());
  EXPECT_EQ(profile_info_provider.GetGivenName(),
            kExpectedTestAccountGivenName);
}

TEST_F(NearbyShareProfileInfoProviderImplTest,
       GivenNameNotUseLoginAccountWhenGivenNameEmpty) {
  AccountManager::Account account;
  account.id = kTestAccountId;
  fake_account_manager().SetAccount(account);
  SetUserGivenName(kProfileGivenName);

  NearbyShareProfileInfoProviderImpl profile_info_provider(
      fake_device_info(), fake_account_manager());
  EXPECT_EQ(profile_info_provider.GetGivenName(), kProfileGivenName);
}

}  // namespace sharing
}  // namespace nearby
