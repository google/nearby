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

#include <map>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/Types.h>

#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/network_manager.h"
#include "internal/platform/implementation/linux/network_manager_active_connection.h"

namespace nearby {
namespace linux {
namespace networkmanager {
std::string ActiveConnection::ActiveConnectionStateReason::ToString() const {
  switch (value) {
    case ActiveConnection::ActiveConnectionStateReason::kStateReasonUnknown:
      return "The reason for the active connection state change is "
             "unknown.";
    case ActiveConnection::ActiveConnectionStateReason::kStateReasonNone:
      return "No reason was given for the active connection state change.";
    case ActiveConnection::ActiveConnectionStateReason::
        kStateReasonUserDisconnected:
      return "The active connection changed state because the user "
             "disconnected it.";
    case ActiveConnection::ActiveConnectionStateReason::
        kStateReasonDeviceDisconnected:
      return "The active connection changed state because the "
             "device it was "
             "using was disconnected.";
    case ActiveConnection::ActiveConnectionStateReason::
        kStateReasonServiceStopped:
      return "The service providing the VPN connection was stopped.";
    case ActiveConnection::ActiveConnectionStateReason::
        kStateReasonIPConfigInvalid:
      return "The IP config of the active connection was invalid.";
    case ActiveConnection::ActiveConnectionStateReason::
        kStateReasonConnectTimeout:
      return "The connection attempt to the VPN service timed out.";
    case ActiveConnection::ActiveConnectionStateReason::
        kStateReasonServiceStartTimeout:
      return "A timeout occurred while starting the service providing the "
             "VPN connection.";
    case ActiveConnection::ActiveConnectionStateReason::
        kStateReasonServiceStartFailed:
      return "Starting the service providing the VPN connection failed.";
    case ActiveConnection::ActiveConnectionStateReason::kStateReasonNoSecrets:
      return "Necessary secrets for the connection were not provided.";
    case ActiveConnection::ActiveConnectionStateReason::kStateReasonLoginFailed:
      return "Authentication to the server failed.";
    case ActiveConnection::ActiveConnectionStateReason::
        kStateReasonConnectionRemoved:
      return "The connection was deleted from settings.";
    case ActiveConnection::ActiveConnectionStateReason::
        kStateReasonDependencyFailed:
      return "Master connection of this connection failed to activate.";
    case ActiveConnection::ActiveConnectionStateReason::
        kStateReasonDeviceRealizeFailed:
      return "Could not create the software device link.";
    case ActiveConnection::ActiveConnectionStateReason::
        kStateReasonDeviceRemoved:
      return "The device this connection depended on disappeared.";
  }
}

std::vector<std::string> ActiveConnection::GetIP4Addresses() {
  sdbus::ObjectPath ip4config_path;
  try {
    ip4config_path = Ip4Config();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(this, "Ip4Config", e);
    return {};
  }

  IP4Config ip4config(system_bus_, ip4config_path);
  std::vector<std::map<std::string, sdbus::Variant>> address_data;
  try {
    address_data = ip4config.AddressData();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(&ip4config, "AddressData", e);
    return {};
  }

  std::vector<std::string> ip4addresses;
  for (auto &data : address_data) {
    if (data.count("address") == 1) {
      ip4addresses.push_back(data["address"]);
    }
  }
  return ip4addresses;
}

std::pair<std::optional<ActiveConnection::ActiveConnectionStateReason>, bool>
ActiveConnection::WaitForConnection(absl::Duration timeout) {
  LOG(INFO) << __func__ << ": Waiting for an update to "
                       << getObjectPath() << "'s state";

  auto state_changed = [this]() {
    this->state_mutex_.AssertReaderHeld();
    return this->state_ == kStateActivated || this->state_ == kStateDeactivated;
  };

  absl::Condition cond(&state_changed);
  auto success = state_mutex_.ReaderLockWhenWithTimeout(cond, timeout);
  auto reason = reason_;
  auto state = state_;
  state_mutex_.ReaderUnlock();

  if (!success) {
    return {reason, true};
  }

  return state == kStateActivated ? std::pair{std::nullopt, false}
                                  : std::pair{std::optional(reason), false};
}
}  // namespace networkmanager
}  // namespace linux
}  // namespace nearby
