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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_V3_DISCOVERY_OPTIONS_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_V3_DISCOVERY_OPTIONS_H_

#include <string>

#include "connections/medium_selector.h"
#include "connections/power_level.h"
#include "connections/strategy.h"

namespace nearby {
namespace connections {
namespace v3 {

struct DiscoveryOptions {
  // Must match advertising strategy to see advertisements.
  Strategy strategy;
  PowerLevel power_level = PowerLevel::kHighPower;
  // Allow Nearby Connections to toggle on Bluetooth radio before starting
  // discovery.
  bool allow_bluetooth_radio_toggling = true;
  // Allow Nearby Connections to toggle on WiFi radio before starting discovery.
  bool allow_wifi_radio_toggling = true;
  std::string fast_advertisement_service_uuid;
  BooleanMediumSelector discovery_mediums;
};

}  // namespace v3
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_V3_DISCOVERY_OPTIONS_H_
