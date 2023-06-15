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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTION_LISTENING_OPTIONS_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTION_LISTENING_OPTIONS_H_

#include <vector>

#include "connections/strategy.h"
#include "internal/interop/device.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
namespace v3 {

struct ConnectionListeningOptions {
  Strategy strategy;
  bool allow_bluetooth_radio_toggling = true;
  bool allow_wifi_radio_toggling = true;
  bool enable_ble_listening = false;
  bool enable_bluetooth_listening = true;
  bool enable_wlan_listening = true;
  bool auto_upgrade_bandwidth = true;
  bool enforce_topology_constraints = true;
  std::vector<location::nearby::proto::connections::Medium> upgrade_mediums;
  std::vector<location::nearby::proto::connections::Medium> listening_mediums;
  nearby::NearbyDevice::Type listening_endpoint_type =
      NearbyDevice::Type::kConnectionsDevice;
};

}  // namespace v3
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTION_LISTENING_OPTIONS_H_
