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

#include "presence/presence_device.h"

#include <string>
#include <vector>

#include "internal/crypto/random.h"
#include "internal/platform/bluetooth_connection_info.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/wifi_lan_connection_info.h"
#include "presence/device_motion.h"

namespace nearby {
namespace presence {

namespace {
std::string GenerateRandomEndpointId() {
  return crypto::RandBytes(kEndpointIdLength);
}
}  // namespace

PresenceDevice::PresenceDevice(DeviceMetadata device_metadata) noexcept
    : discovery_timestamp_(location::nearby::SystemClock::ElapsedRealtime()),
      device_motion_(DeviceMotion()),
      device_metadata_(device_metadata) {
  endpoint_id_ = GenerateRandomEndpointId();
}
PresenceDevice::PresenceDevice(DeviceMotion device_motion,
                               DeviceMetadata device_metadata) noexcept
    : discovery_timestamp_(location::nearby::SystemClock::ElapsedRealtime()),
      device_motion_(device_motion),
      device_metadata_(device_metadata) {
  endpoint_id_ = GenerateRandomEndpointId();
}

std::vector<absl::variant<location::nearby::BluetoothConnectionInfo,
                          location::nearby::WifiLanConnectionInfo>>
PresenceDevice::GetConnectionInfos() const {
  location::nearby::BluetoothConnectionInfo bluetooth_connection_info(
      location::nearby::ByteArray(device_metadata_.bluetooth_mac_address()),
      "Nearby Presence");
  return {bluetooth_connection_info};
}
}  // namespace presence
}  // namespace nearby
