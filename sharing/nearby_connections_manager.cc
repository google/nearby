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

#include "sharing/nearby_connections_manager.h"

#include <memory>
#include <string>

#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

// static
// LINT.IfChange(status_enum)
std::string NearbyConnectionsManager::ConnectionsStatusToString(
    ConnectionsStatus status) {
  switch (status) {
    case ConnectionsStatus::kSuccess:
      return "kSuccess";
    case ConnectionsStatus::kError:
      return "kError";
    case ConnectionsStatus::kOutOfOrderApiCall:
      return "kOutOfOrderApiCall";
    case ConnectionsStatus::kAlreadyHaveActiveStrategy:
      return "kAlreadyHaveActiveStrategy";
    case ConnectionsStatus::kAlreadyAdvertising:
      return "kAlreadyAdvertising";
    case ConnectionsStatus::kAlreadyDiscovering:
      return "kAlreadyDiscovering";
    case ConnectionsStatus::kEndpointIOError:
      return "kEndpointIOError";
    case ConnectionsStatus::kEndpointUnknown:
      return "kEndpointUnknown";
    case ConnectionsStatus::kConnectionRejected:
      return "kConnectionRejected";
    case ConnectionsStatus::kAlreadyConnectedToEndpoint:
      return "kAlreadyConnectedToEndpoint";
    case ConnectionsStatus::kNotConnectedToEndpoint:
      return "kNotConnectedToEndpoint";
    case ConnectionsStatus::kBluetoothError:
      return "kBluetoothError";
    case ConnectionsStatus::kBleError:
      return "kBleError";
    case ConnectionsStatus::kWifiLanError:
      return "kWifiLanError";
    case ConnectionsStatus::kPayloadUnknown:
      return "kPayloadUnknown";
    case ConnectionsStatus::kAlreadyListening:
      return "kAlreadyListening";
    case ConnectionsStatus::kReset:
      return "kReset";
    case ConnectionsStatus::kTimeout:
      return "kTimeout";
    case ConnectionsStatus::kUnknown:
      // fall through
    default:
      return "Unknown";
  }
}
// LINT.ThenChange()

}  // namespace sharing
}  // namespace nearby
