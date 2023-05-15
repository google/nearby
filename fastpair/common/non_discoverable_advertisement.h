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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_NON_DISCOVERABLE_ADVERTISEMENT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_NON_DISCOVERABLE_ADVERTISEMENT_H_

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "fastpair/common/battery_notification.h"

namespace nearby {
namespace fastpair {

// Fast Pair 'Not Discoverable' advertisement. See
// https://developers.google.com/nearby/fast-pair/specifications/service/provider#AdvertisingWhenNotDiscoverable
struct NonDiscoverableAdvertisement {
  // Represents showing UI indication
  enum class Type {
    kShowUi = 0, /* Show UI indication: 0b0000*/
    kNone = 1,
    kHideUi = 2, /* Hide UI indication: 0b0010*/
  };

  NonDiscoverableAdvertisement() = default;
  NonDiscoverableAdvertisement(
      std::vector<uint8_t> account_key_filter, Type type,
      std::vector<uint8_t> salt,
      std::optional<BatteryNotification> battery_notification)
      : account_key_filter(std::move(account_key_filter)),
        type(type),
        salt(std::move(salt)),
        battery_notification(std::move(battery_notification)) {}

  ~NonDiscoverableAdvertisement() = default;

  std::vector<uint8_t> account_key_filter;
  Type type = Type::kNone;
  std::vector<uint8_t> salt;
  std::optional<BatteryNotification> battery_notification;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_NON_DISCOVERABLE_ADVERTISEMENT_H_
