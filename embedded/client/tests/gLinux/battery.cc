// Copyright 2022 Google LLC
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

#include "fakes.h"
#include "nearby.h"
#include "nearby_platform_battery.h"

constexpr nearby_platform_BatteryInfo kDefaultBatteryInfo = {
    .is_charging = true,
    .right_bud_battery_level = 80,
    .left_bud_battery_level = 85,
    .charging_case_battery_level = 90,
    .remaining_time_minutes = 100};
static nearby_platform_BatteryInfo test_battery_info = kDefaultBatteryInfo;
static nearby_platform_status get_battery_info_result = kNearbyStatusOK;

static const nearby_platform_BatteryInterface* battery_interface;
nearby_platform_status nearby_platform_GetBatteryInfo(
    nearby_platform_BatteryInfo* battery_info) {
  (*battery_info) = test_battery_info;
  return get_battery_info_result;
}

void nearby_test_fakes_SetIsCharging(bool charging) {
  test_battery_info.is_charging = charging;
}

void nearby_test_fakes_SetRightBudBatteryLevel(unsigned battery_level) {
  test_battery_info.right_bud_battery_level = battery_level;
}

void nearby_test_fakes_SetLeftBudBatteryLevel(unsigned battery_level) {
  test_battery_info.left_bud_battery_level = battery_level;
}

void nearby_test_fakes_SetChargingCaseBatteryLevel(unsigned battery_level) {
  test_battery_info.charging_case_battery_level = battery_level;
}

void nearby_test_fakes_BatteryTime(uint16_t battery_time) {
  test_battery_info.remaining_time_minutes = battery_time;
}

void nearby_test_fakes_SetGetBatteryInfoResult(nearby_platform_status status) {
  get_battery_info_result = status;
}

nearby_platform_status nearby_platform_BatteryInit(
    nearby_platform_BatteryInterface* callbacks) {
  battery_interface = callbacks;
  test_battery_info = kDefaultBatteryInfo;
  get_battery_info_result = kNearbyStatusOK;
  return kNearbyStatusOK;
}
