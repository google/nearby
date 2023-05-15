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

#include <optional>
#include <utility>
#include <vector>

#include "fastpair/common/constant.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
BatteryInfo::BatteryInfo(bool is_charging)
    : is_charging(is_charging), percentage(std::nullopt) {}

BatteryInfo::BatteryInfo(bool is_charging, int8_t percentage)
    : is_charging(is_charging), percentage(percentage) {}

// static
BatteryInfo BatteryInfo::FromByte(uint8_t byte) {
  // Battery value is in the form 0bSVVVVVVV.
  // S = charging (0b1) or not (0b0).
  // V = value, Ranges from 0-100, or 0bS1111111 if unknown.
  bool is_charging = byte & kBatteryChargingMask;
  uint8_t percentage = byte & kBatteryPercentageMask;
  int8_t percentage_signed = static_cast<int8_t>(percentage);
  if (percentage_signed < 0 || percentage_signed > 100) {
    NEARBY_LOGS(INFO) << __func__ << "Invalid battery percentage.";
    return BatteryInfo(is_charging);
  }
  return BatteryInfo(is_charging, percentage_signed);
}

uint8_t BatteryInfo::ToByte() const {
  // Battery value is in the form 0bSVVVVVVV.
  // S = charging (0b1) or not (0b0).
  // V = value, Ranges from 0-100, or 1111111 if unknown.
  if (!percentage) {
    return is_charging ? kBatteryIsChargingByte : kBatteryNotChargingByte;
  } else {
    return percentage.value() | (is_charging ? kBatteryChargingMask : 0);
  }
}

BatteryNotification::BatteryNotification(
    Type type, const std::vector<BatteryInfo>& battery_infos)
    : type(type), battery_infos(battery_infos) {}

// static
std::optional<BatteryNotification> BatteryNotification::FromBytes(
    const std::vector<uint8_t>& bytes, Type type) {
  if (bytes.size() == 1) {
    // Single component device.
    NEARBY_LOGS(INFO) << __func__ << " : Single component device.";
    std::vector<BatteryInfo> battery_infos = {BatteryInfo::FromByte(bytes[0])};
    return std::make_optional<BatteryNotification>(type, battery_infos);
  } else if (bytes.size() == 3) {
    // True wireless headset expecting 3 bytes - Left bud, Right bud and case.
    NEARBY_LOGS(INFO) << __func__ << " : True wireless headset.";
    std::vector<BatteryInfo> battery_infos = {
        /* left bud info */ BatteryInfo::FromByte(bytes[0]),
        /* right bud info */ BatteryInfo::FromByte(bytes[1]),
        /* case info */ BatteryInfo::FromByte(bytes[2])};
    return std::make_optional<BatteryNotification>(type, battery_infos);
  }
  NEARBY_LOGS(WARNING) << __func__
                       << " : Unexpected battery notification length :"
                       << bytes.size();
  return std::nullopt;
}

}  // namespace fastpair
}  // namespace nearby
