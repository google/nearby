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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_BATTERY_NOTIFICATION_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_BATTERY_NOTIFICATION_H_

#include <cstdint>
#include <optional>
#include <vector>

namespace nearby {
namespace fastpair {

// Fast Pair battery information from notification. See
// https://developers.google.com/nearby/fast-pair/spec#BatteryNotification
struct BatteryInfo {
  BatteryInfo() = default;
  explicit BatteryInfo(bool is_charging);
  BatteryInfo(bool is_charging, int8_t percentage);
  ~BatteryInfo() = default;

  static BatteryInfo FromByte(uint8_t byte);

  uint8_t ToByte() const;

  bool is_charging = false;
  std::optional<int8_t> percentage;
};

// Fast Pair battery notification. See
// https://developers.google.com/nearby/fast-pair/spec#BatteryNotification
struct BatteryNotification {
  // Represents if the provider wants to show an indication
  // of the battery values
  enum class Type {
    kNone = 0,
    kShowUi = 3, /* Show UI indication: 0b0011 */
    kHideUi = 4, /* Hide UI indication: 0b0100 */
  };

  BatteryNotification() = default;
  BatteryNotification(Type type, const std::vector<BatteryInfo>& battery_infos);
  ~BatteryNotification() = default;

  static std::optional<BatteryNotification> FromBytes(
      const std::vector<uint8_t>& bytes, Type type);

  Type type = Type::kNone;
  std::vector<BatteryInfo> battery_infos;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_BATTERY_NOTIFICATION_H_
