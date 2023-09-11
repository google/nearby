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
std::ostream &operator<<(
    std::ostream &stream,
    const ActiveConnection::ActiveConnectionStateReason &reason) {
  switch (reason) {
    case ActiveConnection::kStateReasonUnknown:
      return stream << "The reason for the active connection state change is "
                       "unknown.";
    case ActiveConnection::kStateReasonNone:
      return stream
             << "No reason was given for the active connection state change.";
    case ActiveConnection::kStateReasonUserDisconnected:
      return stream << "The active connection changed state because the user "
                       "disconnected it.";
    case ActiveConnection::kStateReasonDeviceDisconnected:
      return stream << "The active connection changed state because the "
                       "device it was "
                       "using was disconnected.";
    case ActiveConnection::kStateReasonServiceStopped:
      return stream << "The service providing the VPN connection was stopped.";
    case ActiveConnection::kStateReasonIPConfigInvalid:
      return stream << "The IP config of the active connection was invalid.";
    case ActiveConnection::kStateReasonConnectTimeout:
      return stream << "The connection attempt to the VPN service timed out.";
    case ActiveConnection::kStateReasonServiceStartTimeout:
      return stream
             << "A timeout occurred while starting the service providing the "
                "VPN connection.";
    case ActiveConnection::kStateReasonServiceStartFailed:
      return stream
             << "Starting the service providing the VPN connection failed.";
    case ActiveConnection::kStateReasonNoSecrets:
      return stream
             << "Necessary secrets for the connection were not provided.";
    case ActiveConnection::kStateReasonLoginFailed:
      return stream << "Authentication to the server failed.";
    case ActiveConnection::kStateReasonConnectionRemoved:
      return stream << "The connection was deleted from settings.";
    case ActiveConnection::kStateReasonDependencyFailed:
      return stream
             << "Master connection of this connection failed to activate.";
    case ActiveConnection::kStateReasonDeviceRealizeFailed:
      return stream << "Could not create the software device link.";
    case ActiveConnection::kStateReasonDeviceRemoved:
      return stream << "The device this connection depended on disappeared.";
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
  NEARBY_LOGS(VERBOSE) << __func__ << ": Waiting for an update to "
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
