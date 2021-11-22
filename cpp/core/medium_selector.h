// Copyright 2020 Google LLC
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
#ifndef CORE_MEDIUM_SELECTOR_H_
#define CORE_MEDIUM_SELECTOR_H_

#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

using Medium = location::nearby::proto::connections::Medium;

// Generic type: allows definition of a feature T for every Medium.
template <typename T>
struct MediumSelector {
  T bluetooth;
  T ble;
  T web_rtc;
  T wifi_lan;

  constexpr MediumSelector() = default;
  constexpr MediumSelector(const MediumSelector&) = default;
  constexpr MediumSelector& operator=(const MediumSelector&) = default;
  constexpr bool Any(T value) const {
    return bluetooth == value || ble == value || web_rtc == value ||
           wifi_lan == value;
  }

  constexpr bool All(T value) const {
    return bluetooth == value && ble == value && web_rtc == value &&
           wifi_lan == value;
  }

  constexpr int Count(T value) const {
    int count = 0;
    if (bluetooth == value) count++;
    if (ble == value) count++;
    if (wifi_lan == value) count++;
    if (web_rtc == value) count++;
    return count;
  }

  constexpr MediumSelector& SetAll(T value) {
    bluetooth = value;
    ble = value;
    web_rtc = value;
    wifi_lan = value;
    return *this;
  }

  std::vector<Medium> GetMediums(T value) const {
    std::vector<Medium> mediums;
    // Mediums are sorted in order of decreasing preference.
    if (wifi_lan == value) mediums.push_back(Medium::WIFI_LAN);
    if (web_rtc == value) mediums.push_back(Medium::WEB_RTC);
    if (bluetooth == value) mediums.push_back(Medium::BLUETOOTH);
    if (ble == value) mediums.push_back(Medium::BLE);
    return mediums;
  }
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_MEDIUM_SELECTOR_H_
