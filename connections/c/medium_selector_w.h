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
#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_C_MEDIUM_SELECTOR_W_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_C_MEDIUM_SELECTOR_W_H_

#include "proto/connections_enums.pb.h"

namespace nearby::windows {

using MediumW = ::location::nearby::proto::connections::Medium;

// Generic type: allows definition of a feature T for every Medium.
template <typename T>
struct MediumSelectorW {
  T bluetooth;
  T ble;
  T web_rtc;
  T wifi_lan;
  T wifi_hotspot;
  T wifi_direct;

  constexpr MediumSelectorW() = default;
  constexpr MediumSelectorW(const MediumSelectorW&) = default;
  constexpr MediumSelectorW& operator=(const MediumSelectorW&) = default;
  constexpr bool Any(const T& value) const {
    return bluetooth == value || ble == value || web_rtc == value ||
           wifi_lan == value || wifi_hotspot == value || wifi_direct == value;
  }

  constexpr bool All(const T& value) const {
    return bluetooth == value && ble == value && web_rtc == value &&
           wifi_lan == value && wifi_hotspot == value && wifi_direct == value;
  }

  constexpr int Count(const T& value) const {
    int count = 0;
    if (bluetooth == value) ++count;
    if (ble == value) ++count;
    if (wifi_lan == value) ++count;
    if (wifi_hotspot == value) ++count;
    if (wifi_direct == value) ++count;
    if (web_rtc == value) ++count;
    return count;
  }

  constexpr MediumSelectorW& SetAll(const T& value) {
    bluetooth = value;
    ble = value;
    web_rtc = value;
    wifi_lan = value;
    wifi_hotspot = value;
    wifi_direct = value;
    return *this;
  }

  std::vector<MediumW> GetMediums(const T& value) const {
    std::vector<MediumW> mediums;
    // Mediums are sorted in order of decreasing preference.
    if (wifi_lan == value) mediums.push_back(MediumW::WIFI_LAN);
    if (wifi_direct == value) mediums.push_back(MediumW::WIFI_DIRECT);
    if (wifi_hotspot == value) mediums.push_back(MediumW::WIFI_HOTSPOT);
    if (web_rtc == value) mediums.push_back(MediumW::WEB_RTC);
    if (bluetooth == value) mediums.push_back(MediumW::BLUETOOTH);
    if (ble == value) mediums.push_back(MediumW::BLE);
    return mediums;
  }
};

// Feature On/Off switch for mediums.
using BooleanMediumSelectorW = MediumSelectorW<bool>;

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_C_MEDIUM_SELECTOR_W_H_
