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

#include <vector>

#include "proto/connections_enums.pb.h"

namespace nearby::windows {

using MediumW = ::location::nearby::proto::connections::Medium;

// Feature On/Off switch for mediums.
struct BooleanMediumSelectorW {
  bool bluetooth;
  bool ble;
  bool web_rtc;
  bool wifi_lan;
  bool wifi_hotspot;
  bool wifi_direct;

  BooleanMediumSelectorW() = default;
  constexpr BooleanMediumSelectorW(const BooleanMediumSelectorW&) = default;
  constexpr BooleanMediumSelectorW& operator=(const BooleanMediumSelectorW&) =
      default;
  constexpr bool Any(const bool value) const {
    return bluetooth == value || ble == value || web_rtc == value ||
           wifi_lan == value || wifi_hotspot == value || wifi_direct == value;
  }

  constexpr bool All(const bool value) const {
    return bluetooth == value && ble == value && web_rtc == value &&
           wifi_lan == value && wifi_hotspot == value && wifi_direct == value;
  }

  constexpr int Count(const bool value) const {
    int count = 0;
    if (bluetooth == value) ++count;
    if (ble == value) ++count;
    if (wifi_lan == value) ++count;
    if (wifi_hotspot == value) ++count;
    if (wifi_direct == value) ++count;
    if (web_rtc == value) ++count;
    return count;
  }

  constexpr BooleanMediumSelectorW& SetAll(const bool value) {
    bluetooth = value;
    ble = value;
    web_rtc = value;
    wifi_lan = value;
    wifi_hotspot = value;
    wifi_direct = value;
    return *this;
  }

  std::vector<MediumW> GetMediums(const bool value) const {
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

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_C_MEDIUM_SELECTOR_W_H_
