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

#ifndef CORE_V2_OPTIONS_H_
#define CORE_V2_OPTIONS_H_

#include "core_v2/strategy.h"
#include "platform_v2/base/byte_array.h"
#include "proto/connections_enums.pb.h"
#include "proto/connections_enums.pb.h"

namespace location {
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

// Feature On/Off switch for mediums.
using BooleanMediumSelector = MediumSelector<bool>;

// Represents the various power levels that can be used, on mediums that support
// it.
enum class PowerLevel {
  kHighPower = 0,
  kLowPower = 1,
};

// Connection Options: used for both Advertising and Discovery.
// All fields are mutable, to make the type copy-assignable.
struct ConnectionOptions {
  Strategy strategy;
  BooleanMediumSelector allowed{BooleanMediumSelector().SetAll(true)};
  bool auto_upgrade_bandwidth;
  bool enforce_topology_constraints;
  bool low_power;
  bool enable_bluetooth_listening;
  ByteArray remote_bluetooth_mac_address;
  std::string fast_advertisement_service_uuid;
  // Verify if  ConnectionOptions is in a not-initialized (Empty) state.
  bool Empty() const { return strategy.IsNone(); }
  // Bring  ConnectionOptions to a not-initialized (Empty) state.
  void Clear() { strategy.Clear(); }
  // Returns a copy, but if no mediums are allowed, allowes all mediums.
  ConnectionOptions CompatibleOptions() const {
    ConnectionOptions result = *this;
    if (!allowed.Any(true)) {
      result.allowed.SetAll(true);
    }
    return result;
  }
  std::vector<Medium> GetMediums() const { return allowed.GetMediums(true); }
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_OPTIONS_H_
