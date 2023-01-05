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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_DEVICE_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_DEVICE_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "internal/platform/ble_connection_info.h"
#include "internal/platform/bluetooth_connection_info.h"
#include "internal/platform/wifi_lan_connection_info.h"

namespace nearby {

using ConnectionInfoVariant =
    absl::variant<BleConnectionInfo, BluetoothConnectionInfo,
                  WifiLanConnectionInfo>;

class NearbyDevice {
 public:
  enum Type {
    kUnknownDevice = 0,
    kConnectionsDevice = 1,
    kPresenceDevice = 2,
  };
  NearbyDevice() = default;
  virtual ~NearbyDevice() = default;
  NearbyDevice(NearbyDevice&&) = default;
  NearbyDevice& operator=(NearbyDevice&&) = default;
  NearbyDevice(const NearbyDevice&) = delete;
  NearbyDevice& operator=(const NearbyDevice&) = delete;
  virtual absl::string_view GetEndpointId() const = 0;
  virtual absl::string_view GetEndpointInfo() const = 0;
  // We will be adding more ConnectionInfo types to this variant as they are
  // implemented.
  virtual std::vector<ConnectionInfoVariant> GetConnectionInfos() const = 0;
  virtual Type GetType() const { return Type::kUnknownDevice; }
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_IMPLEMENTATION_DEVICE_H_
