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

#include "fastpair/common/account_key_filter.h"

#include <optional>
#include <vector>

#include "gtest/gtest.h"
#include "fastpair/common/battery_notification.h"
#include "fastpair/common/non_discoverable_advertisement.h"

namespace nearby {
namespace fastpair {
namespace {
// Test data comes from:
// https://developers.google.com/nearby/fast-pair/specifications/appendix/testcases#test_cases

class AccountKeyFilterTest : public testing::Test {
 protected:
  const std::vector<uint8_t> salt_{0xC7, 0xC8};
  const std::vector<uint8_t> account_key_1_{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                            0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                            0xCC, 0xDD, 0xEE, 0xFF};
  const std::vector<uint8_t> account_key_2_{0x11, 0x11, 0x22, 0x22, 0x33, 0x33,
                                            0x44, 0x44, 0x55, 0x55, 0x66, 0x66,
                                            0x77, 0x77, 0x88, 0x88};

  const std::vector<uint8_t> filter_1_{0x02, 0x0C, 0x80, 0x2A};
  const std::vector<uint8_t> filter_2_{0x84, 0x4A, 0x62, 0x20, 0x8B};
  const std::vector<uint8_t> filter_1_and_2_{0x84, 0x4A, 0x62, 0x20, 0x8B};

  const std::vector<uint8_t> battery_data_{0b00110011, 0b01000000, 0b01000000,
                                           0b01000000};

  const std::vector<uint8_t> filter_1_with_battery_{0x01, 0x01, 0x46, 0x0A};
  const std::vector<uint8_t> filter_2_with_battery_{0x46, 0x15, 0x24, 0xD0,
                                                    0x08};
};

TEST_F(AccountKeyFilterTest, ConstrutorWithNonDiscoverableAdvertisement) {
  NonDiscoverableAdvertisement non_discoverable_advertisement(
      filter_1_, NonDiscoverableAdvertisement::Type::kShowUi, salt_, {});
  EXPECT_TRUE(AccountKeyFilter(non_discoverable_advertisement)
                  .IsPossiblyInSet(AccountKey(account_key_1_)));
  NonDiscoverableAdvertisement non_discoverable_advertisement_with_battery(
      filter_1_with_battery_, NonDiscoverableAdvertisement::Type::kShowUi,
      salt_,
      BatteryNotification::FromBytes({0b01000000, 0b01000000, 0b01000000},
                                     BatteryNotification::Type::kShowUi));
  EXPECT_TRUE(AccountKeyFilter(non_discoverable_advertisement_with_battery)
                  .IsPossiblyInSet(AccountKey(account_key_1_)));
}

TEST_F(AccountKeyFilterTest, EmptyAccountKeyFilter) {
  AccountKeyFilter filter({}, {});

  EXPECT_FALSE(filter.IsPossiblyInSet(AccountKey(account_key_1_)));
  EXPECT_FALSE(filter.IsPossiblyInSet(AccountKey(account_key_2_)));
}

TEST_F(AccountKeyFilterTest, EmptyAccountKey) {
  EXPECT_FALSE(
      AccountKeyFilter(filter_1_, salt_).IsPossiblyInSet(AccountKey("")));
}

TEST_F(AccountKeyFilterTest, SingleAccountKey) {
  EXPECT_TRUE(AccountKeyFilter(filter_1_, salt_)
                  .IsPossiblyInSet(AccountKey(AccountKey(account_key_1_))));
  EXPECT_TRUE(AccountKeyFilter(filter_2_, salt_)
                  .IsPossiblyInSet(AccountKey(account_key_2_)));
}

TEST_F(AccountKeyFilterTest, TwoAccountKeys) {
  AccountKeyFilter filter(filter_1_and_2_, salt_);
  EXPECT_TRUE(filter.IsPossiblyInSet(AccountKey(account_key_1_)));
  EXPECT_TRUE(filter.IsPossiblyInSet(AccountKey(account_key_2_)));
}

TEST_F(AccountKeyFilterTest, MissingAccountKey) {
  const std::vector<uint8_t> bytes{0x12, 0x22, 0x33, 0x44, 0x55, 0x66,
                                   0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                   0xCC, 0xDD, 0xEE, 0xFF};
  AccountKey account_key(bytes);
  EXPECT_FALSE(AccountKeyFilter(filter_1_, salt_).IsPossiblyInSet(account_key));
  EXPECT_FALSE(
      AccountKeyFilter(filter_1_and_2_, salt_).IsPossiblyInSet(account_key));
}

TEST_F(AccountKeyFilterTest, AccountKeyWithBatteryData) {
  std::vector<uint8_t> salt_1 = salt_;
  for (auto& byte : battery_data_) salt_1.push_back(byte);

  EXPECT_TRUE(AccountKeyFilter(filter_1_with_battery_, salt_1)
                  .IsPossiblyInSet(AccountKey(account_key_1_)));

  std::vector<uint8_t> salt_2 = salt_;
  for (auto& byte : battery_data_) salt_2.push_back(byte);

  EXPECT_TRUE(AccountKeyFilter(filter_2_with_battery_, salt_2)
                  .IsPossiblyInSet(AccountKey(account_key_2_)));
}

TEST_F(AccountKeyFilterTest, SassEnabledPeripheral) {
  // Value source: b/243855406#comment24
  const std::vector<uint8_t> bytes{0x06, 0x3F, 0xC1, 0x8C, 0x63, 0xDC,
                                   0x75, 0x1A, 0xE8, 0x1A, 0xCF, 0x65,
                                   0x10, 0x15, 0x1D, 0xB0};
  AccountKey account_key_3(bytes);
  const std::vector<uint8_t> filter4{0x19, 0x23, 0x50, 0xE8, 0x37,
                                     0x68, 0xF0, 0x65, 0x22};
  const std::vector<uint8_t> salt4{0xD7, 0xDE};
  const std::vector<uint8_t> batteryData4{0x33, 0xE4, 0xE4, 0x64};
  std::vector<uint8_t> salt_values{};
  salt_values.insert(salt_values.end(), salt4.begin(), salt4.end());
  salt_values.insert(salt_values.end(), batteryData4.begin(),
                     batteryData4.end());
  EXPECT_TRUE(
      AccountKeyFilter(filter4, salt_values).IsPossiblyInSet(account_key_3));
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
