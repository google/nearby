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

#ifndef NEARBY_PLATFORM_BATTERY_H
#define NEARBY_PLATFORM_BATTERY_H

#include "nearby.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
  // Set to true if the device is charging.
  bool is_charging;
  // Battery level of the right bud. 0 - 100 range.
  uint8_t right_bud_battery_level;
  // Battery level of the left bud. 0 - 100 range.
  uint8_t left_bud_battery_level;
  // Battery level of the charging case . 0 - 100 range.
  uint8_t charging_case_battery_level;
  // Battery remaining time in minutes, 0-65335 range.
  uint16_t remaining_time_minutes;
} nearby_platform_BatteryInfo;

typedef struct {
  void (*on_battery_changed)(void);
} nearby_platform_BatteryInterface;

// Gets battery and charging info
//
// battery_info - Battery status structure.
nearby_platform_status nearby_platform_GetBatteryInfo(
    nearby_platform_BatteryInfo* battery_info);

// Initializes battery module
//
// battery_interface - Battery status callback events.
nearby_platform_status nearby_platform_BatteryInit(
    nearby_platform_BatteryInterface* battery_interface);
#ifdef __cplusplus
}
#endif

#endif /* NEARBY_PLATFORM_BATTERY_H */
