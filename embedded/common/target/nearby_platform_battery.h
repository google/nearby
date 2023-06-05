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

// One byte per left, right and charging case
#define NEARBY_BATTERY_LEVELS_SIZE 3

#define NEARBY_PLATFORM_BATTERY_INFO_CHARGING (1 << 7)
#define NEARBY_PLATFORM_BATTERY_INFO_NOT_CHARGING 0
// In the battery values, the highest bit indicates charging, the lower 7 are
// the battery level
#define NEARBY_PLATFORM_BATTERY_LEVEL_MASK 0x7F

enum NearbyBatteryLevelIndex {
  kNearbyLeftBudBatteryLevel = 0,
  kNearbyRightBudBatteryLevel = 1,
  kNearbyChargingCaseBatteryLevel = 2
};

#define NEARBY_PLATFORM_SET_BATTERY_LEVEL(level, charging) \
  (charging << 7) | (level & NEARBY_PLATFORM_BATTERY_LEVEL_MASK)

#define NEARBY_PLATFORM_GET_BATTERY_LEVEL(level) \
  (level & NEARBY_PLATFORM_BATTERY_LEVEL_MASK)

#define NEARBY_PLATFORM_GET_IS_CHARGING(level) \
  (level & NEARBY_PLATFORM_BATTERY_INFO_CHARGING)

typedef struct {
  // Bit 7  : Set to 1 if the device is charging.
  // Bit 6-0: Battery level 0 - 100 range.
  uint8_t battery_level[NEARBY_BATTERY_LEVELS_SIZE];
#if NEARBY_BATTERY_REMAINING_TIME
  // Battery remaining time in minutes, 0-65335 range.
  uint16_t remaining_time_minutes;
#endif /* NEARBY_BATTERY_REMAINING_TIME */
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
