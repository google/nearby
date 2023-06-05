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

#ifdef NEARBY_SPOT_BATTERY_LEVEL_INDICATION
#include "nearby_spot.h"
#endif /* NEARBY_SPOT_BATTERY_LEVEL_INDICATION */

constexpr nearby_platform_BatteryInfo kDefaultBatteryInfo = {
#if NEARBY_BATTERY_LEVELS_SIZE == 1
                                                             {NEARBY_PLATFORM_SET_BATTERY_LEVEL(85, true)}
#elif NEARBY_BATTERY_LEVELS_SIZE == 2
                                                             {NEARBY_PLATFORM_SET_BATTERY_LEVEL(85, true), \
                                                              NEARBY_PLATFORM_SET_BATTERY_LEVEL(80, true)}
#elif NEARBY_BATTERY_LEVELS_SIZE == 3
                                                             {NEARBY_PLATFORM_SET_BATTERY_LEVEL(85, true), \
                                                              NEARBY_PLATFORM_SET_BATTERY_LEVEL(80, true), \
                                                              NEARBY_PLATFORM_SET_BATTERY_LEVEL(90, true)}
#endif /* NEARBY_BATTERY_LEVELS_SIZE */
#if NEARBY_BATTERY_REMAINING_TIME
                                                             ,100
#endif /* NEARBY_BATTERY_REMAINING_TIME */
                                                             };
static nearby_platform_BatteryInfo test_battery_info = kDefaultBatteryInfo;
static nearby_platform_status get_battery_info_result = kNearbyStatusOK;

static const nearby_platform_BatteryInterface* battery_interface;
nearby_platform_status nearby_platform_GetBatteryInfo(
    nearby_platform_BatteryInfo* battery_info) {
  (*battery_info) = test_battery_info;
  return get_battery_info_result;
}

void nearby_test_fakes_SetIsCharging(bool charging) {
  uint8_t level = 0;
  uint8_t charging_bit = NEARBY_PLATFORM_BATTERY_INFO_NOT_CHARGING;
  // If charging is true set the charging bit to 1
  if (charging) {
    charging_bit = NEARBY_PLATFORM_BATTERY_INFO_CHARGING;
  }

  level = NEARBY_PLATFORM_GET_BATTERY_LEVEL(test_battery_info.battery_level[kNearbyLeftBudBatteryLevel]);
  test_battery_info.battery_level[kNearbyLeftBudBatteryLevel] = charging_bit | level;

#if NEARBY_BATTERY_LEVELS_SIZE >= 2
  level = NEARBY_PLATFORM_GET_BATTERY_LEVEL(test_battery_info.battery_level[kNearbyRightBudBatteryLevel]);
  test_battery_info.battery_level[kNearbyRightBudBatteryLevel] = charging_bit | level;
#endif

#if NEARBY_BATTERY_LEVELS_SIZE >= 3
  level = NEARBY_PLATFORM_GET_BATTERY_LEVEL(test_battery_info.battery_level[kNearbyChargingCaseBatteryLevel]);
  test_battery_info.battery_level[kNearbyChargingCaseBatteryLevel] = charging_bit | level;
#endif
}

#if NEARBY_BATTERY_LEVELS_SIZE >= 2
void nearby_test_fakes_SetRightBudBatteryLevel(unsigned battery_level, bool charging) {
  test_battery_info.battery_level[kNearbyRightBudBatteryLevel] =
    NEARBY_PLATFORM_SET_BATTERY_LEVEL(battery_level, charging);
}
#endif

void nearby_test_fakes_SetLeftBudBatteryLevel(unsigned battery_level, bool charging) {
  test_battery_info.battery_level[kNearbyLeftBudBatteryLevel] =
    NEARBY_PLATFORM_SET_BATTERY_LEVEL(battery_level, charging);
}

#if NEARBY_BATTERY_LEVELS_SIZE >= 3
void nearby_test_fakes_SetChargingCaseBatteryLevel(unsigned battery_level, bool charging) {
  test_battery_info.battery_level[kNearbyChargingCaseBatteryLevel] =
    NEARBY_PLATFORM_SET_BATTERY_LEVEL(battery_level, charging);
}
#endif

#ifdef NEARBY_SPOT_BATTERY_LEVEL_INDICATION
void nearby_test_fakes_SetMainBatteryLevel(unsigned battery_level) {
  test_battery_info.battery_level[0] =
    NEARBY_PLATFORM_SET_BATTERY_LEVEL(battery_level, false);
}
#endif /* NEARBY_SPOT_BATTERY_LEVEL_INDICATION */

#if NEARBY_BATTERY_REMAINING_TIME
void nearby_test_fakes_BatteryTime(uint16_t battery_time) {
  test_battery_info.remaining_time_minutes = battery_time;
}
#endif /* NEARBY_BATTERY_REMAINING_TIME */

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
