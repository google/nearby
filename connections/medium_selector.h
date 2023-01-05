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

namespace nearby {
namespace connections {

using Medium = ::location::nearby::proto::connections::Medium;

// Generic type: allows definition of a feature T for every Medium.
template <typename T>
struct MediumSelector {
  T bluetooth;
  T ble;
  T web_rtc;
  T wifi_lan;
  T wifi_hotspot;
  T wifi_direct;

  constexpr bool Any(T value) const {
    return bluetooth == value || ble == value || web_rtc == value ||
           wifi_lan == value || wifi_hotspot == value || wifi_direct == value;
  }

  constexpr bool All(T value) const {
    return bluetooth == value && ble == value && web_rtc == value &&
           wifi_lan == value && wifi_hotspot == value && wifi_direct == value;
  }

  constexpr int Count(T value) const {
    int count = 0;
    if (bluetooth == value) count++;
    if (ble == value) count++;
    if (wifi_lan == value) count++;
    if (wifi_hotspot == value) count++;
    if (wifi_direct == value) count++;
    if (web_rtc == value) count++;
    return count;
  }

  constexpr MediumSelector& SetAll(T value) {
    bluetooth = value;
    ble = value;
    web_rtc = value;
    wifi_lan = value;
    wifi_hotspot = value;
    wifi_direct = value;
    return *this;
  }

  std::vector<Medium> GetMediums(T value) const {
    std::vector<Medium> mediums;
    // Mediums are sorted in order of decreasing preference.
    if (wifi_lan == value) mediums.push_back(Medium::WIFI_LAN);
    if (wifi_direct == value) mediums.push_back(Medium::WIFI_DIRECT);
    if (wifi_hotspot == value) mediums.push_back(Medium::WIFI_HOTSPOT);
    if (web_rtc == value) mediums.push_back(Medium::WEB_RTC);
    if (bluetooth == value) mediums.push_back(Medium::BLUETOOTH);
    if (ble == value) mediums.push_back(Medium::BLE);
    return mediums;
  }
};

using BooleanMediumSelector = MediumSelector<bool>;

}  // namespace connections
}  // namespace nearby

#endif  // CORE_MEDIUM_SELECTOR_H_
