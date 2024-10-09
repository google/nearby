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

#include <vector>

#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {

using Medium = ::location::nearby::proto::connections::Medium;

struct BooleanMediumSelector {
  bool bluetooth = false;
  bool ble = false;
  bool web_rtc_no_cellular = false;
  bool web_rtc = false;
  bool wifi_lan = false;
  bool wifi_hotspot = false;
  bool wifi_direct = false;


  constexpr bool Any(bool value) const {
    return bluetooth == value || ble == value || web_rtc_no_cellular == value ||
           web_rtc == value || wifi_lan == value || wifi_hotspot == value ||
           wifi_direct == value;
  }

  constexpr bool All(bool value) const {
    return bluetooth == value && ble == value &&
           (web_rtc == value || web_rtc_no_cellular == value) &&
           wifi_lan == value && wifi_hotspot == value && wifi_direct == value;
  }

  constexpr int Count(bool value) const {
    int count = 0;
    if (bluetooth == value) count++;
    if (ble == value) count++;
    if (wifi_lan == value) count++;
    if (wifi_hotspot == value) count++;
    if (wifi_direct == value) count++;
    if (web_rtc == value || web_rtc_no_cellular == value) count++;
    return count;
  }

  constexpr BooleanMediumSelector& SetAll(bool value) {
    bluetooth = value;
    ble = value;
    web_rtc = value;
    wifi_lan = value;
    wifi_hotspot = value;
    wifi_direct = value;
    return *this;
  }

  std::vector<Medium> GetMediums(bool value) const {
    std::vector<Medium> mediums;
    // Mediums are sorted in order of decreasing preference.
    if (wifi_lan == value) mediums.push_back(Medium::WIFI_LAN);
    if (wifi_direct == value) mediums.push_back(Medium::WIFI_DIRECT);
    if (wifi_hotspot == value) mediums.push_back(Medium::WIFI_HOTSPOT);
    // if both web_rtc and web_rtc_no_cellular are true/false, we only add one
    // web_rtc medium.
    if (web_rtc == value && web_rtc_no_cellular == value) {
      mediums.push_back(Medium::WEB_RTC);
    } else {
      if (web_rtc == value) mediums.push_back(Medium::WEB_RTC);
      if (web_rtc_no_cellular == value)
        mediums.push_back(Medium::WEB_RTC_NON_CELLULAR);
    }
    if (bluetooth == value) mediums.push_back(Medium::BLUETOOTH);
    if (ble == value) mediums.push_back(Medium::BLE);
    return mediums;
  }
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_MEDIUM_SELECTOR_H_
