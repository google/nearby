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

#include "fastpair/common/battery_notification.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace {
// Test data comes from:
// https://developers.google.com/nearby/fast-pair/specifications/appendix/testcases#test_cases

TEST(BatteryNotificationTest, TestBatteryInfo) {
  // Tests default constructor
  BatteryInfo battery_info_1;
  EXPECT_FALSE(battery_info_1.is_charging);

  // Tests Constructor with is_charging
  BatteryInfo battery_info_2(true);
  EXPECT_TRUE(battery_info_2.is_charging);
  // Tests ToByte and FromByte
  BatteryInfo battery_info_3 = BatteryInfo::FromByte(battery_info_2.ToByte());
  EXPECT_TRUE(battery_info_3.is_charging);
  EXPECT_FALSE(battery_info_3.percentage.has_value());

  // Tests Constructor with is_charging and percentage value
  BatteryInfo battery_info_4(true, 80);
  EXPECT_TRUE(battery_info_4.is_charging);
  EXPECT_EQ(battery_info_4.percentage.value(), 80);
  // Tests ToByte and FromByte
  BatteryInfo battery_info_5 = BatteryInfo::FromByte(battery_info_4.ToByte());
  EXPECT_TRUE(battery_info_5.is_charging);
  EXPECT_EQ(battery_info_5.percentage.value(), 80);

  // Tests Constructor with is_charging and wrong percentage value
  BatteryInfo battery_info_6(true, 110);
  // Tests ToByte and FromByte
  BatteryInfo battery_info_7 = BatteryInfo::FromByte(battery_info_6.ToByte());
  EXPECT_TRUE(battery_info_7.is_charging);
  EXPECT_FALSE(battery_info_7.percentage.has_value());

  // Tests FromByte with battery percentage = 0
  BatteryInfo battery_info_8 = BatteryInfo::FromByte(0);
  EXPECT_FALSE(battery_info_8.is_charging);
  EXPECT_EQ(battery_info_8.percentage.value(), 0);

  BatteryInfo battery_info_10 = BatteryInfo::FromByte(0x80);
  EXPECT_TRUE(battery_info_10.is_charging);
  EXPECT_EQ(battery_info_10.percentage.value(), 0);

  // Tests FromByte with battery percentage = 100
  BatteryInfo battery_info_9 = BatteryInfo::FromByte(100);
  EXPECT_FALSE(battery_info_9.is_charging);
  EXPECT_EQ(battery_info_9.percentage.value(), 100);

  BatteryInfo battery_info_11 = BatteryInfo::FromByte(0x80 | 100);
  EXPECT_TRUE(battery_info_11.is_charging);
  EXPECT_EQ(battery_info_11.percentage.value(), 100);

  // Test battery with invalid battery percentage
  BatteryInfo battery_info_12 = BatteryInfo::FromByte(0x7F);
  EXPECT_FALSE(battery_info_12.is_charging);
  EXPECT_FALSE(battery_info_12.percentage.has_value());

  BatteryInfo battery_info_13 = BatteryInfo::FromByte(0xFF);
  EXPECT_TRUE(battery_info_13.is_charging);
  EXPECT_FALSE(battery_info_13.percentage.has_value());
}

TEST(BatteryNotificationTest, TestBatteryNotificationDefaultConstructor) {
  // Tests default constructor
  BatteryNotification battery_notification;
  EXPECT_EQ(battery_notification.type, BatteryNotification::Type::kNone);
  EXPECT_EQ(battery_notification.battery_infos.size(), 0);
}

TEST(BatteryNotificationTest, TestBatteryNotificationForSingleComponentDevice) {
  std::vector<BatteryInfo> battery_infos = {BatteryInfo(false, 70)};
  BatteryNotification battery_notification(BatteryNotification::Type::kShowUi,
                                           battery_infos);
  EXPECT_EQ(battery_notification.type, BatteryNotification::Type::kShowUi);
  EXPECT_FALSE(battery_notification.battery_infos.at(0).is_charging);
  EXPECT_EQ(battery_notification.battery_infos.at(0).percentage.value(), 70);
}

TEST(BatteryNotificationTest, TestBatteryNotificationForTrueWirelessHeadset) {
  std::vector<BatteryInfo> battery_infos = {
      BatteryInfo(false, 70), BatteryInfo(false, 80), BatteryInfo(true, 90)};
  BatteryNotification battery_notification(BatteryNotification::Type::kShowUi,
                                           battery_infos);
  EXPECT_EQ(battery_notification.type, BatteryNotification::Type::kShowUi);
  // Left bud
  EXPECT_FALSE(battery_notification.battery_infos.at(0).is_charging);
  EXPECT_EQ(battery_notification.battery_infos.at(0).percentage.value(), 70);
  // Right bud
  EXPECT_FALSE(battery_notification.battery_infos.at(1).is_charging);
  EXPECT_EQ(battery_notification.battery_infos.at(1).percentage.value(), 80);
  // Case
  EXPECT_TRUE(battery_notification.battery_infos.at(2).is_charging);
  EXPECT_EQ(battery_notification.battery_infos.at(2).percentage.value(), 90);
}

TEST(BatteryNotificationTest,
     TestBatteryNotificationFromBytesForSingleComponentDevice) {
  const std::vector<uint8_t> batteryData{0b01000000};
  BatteryNotification battery_notification =
      BatteryNotification::FromBytes(batteryData,
                                     BatteryNotification::Type::kShowUi)
          .value();
  EXPECT_EQ(battery_notification.type, BatteryNotification::Type::kShowUi);
  EXPECT_EQ(battery_notification.battery_infos.at(0).percentage.value(), 64);
}

TEST(BatteryNotificationTest,
     TestBatteryNotificationFromBytesForTrueWirelessHeadset) {
  const std::vector<uint8_t> batteryData{0b01000000, 0b01000000, 0b01000000};
  BatteryNotification battery_notification =
      BatteryNotification::FromBytes(batteryData,
                                     BatteryNotification::Type::kShowUi)
          .value();
  EXPECT_EQ(battery_notification.type, BatteryNotification::Type::kShowUi);
  EXPECT_EQ(battery_notification.battery_infos.at(0).percentage.value(), 64);
  EXPECT_EQ(battery_notification.battery_infos.at(1).percentage.value(), 64);
  EXPECT_EQ(battery_notification.battery_infos.at(2).percentage.value(), 64);
}

TEST(BatteryNotificationTest, TestBatteryNotificationFromWrongBytes) {
  const std::vector<uint8_t> batteryDataWithWrongLenth{0b01000000, 0b01000000};
  EXPECT_FALSE(
      BatteryNotification::FromBytes(batteryDataWithWrongLenth,
                                     BatteryNotification::Type::kShowUi)
          .has_value());
}
}  // namespace

}  // namespace fastpair
}  // namespace nearby
