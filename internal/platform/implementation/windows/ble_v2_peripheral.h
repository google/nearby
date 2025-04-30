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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_PERIPHERAL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_PERIPHERAL_H_

#include <string>
#include <utility>

#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/mac_address.h"

namespace nearby {
namespace windows {

// BLE peripheral implementation for BLE version 2. Must contain enough data
// about a particular BLE device to connect to its GATT server.
class BleV2Peripheral : public api::ble_v2::BlePeripheral {
 public:
  using UniqueId = api::ble_v2::BlePeripheral::UniqueId;
  explicit BleV2Peripheral(MacAddress address)
      : mac_address_(std::move(address)) {}
  ~BleV2Peripheral() override = default;

  // Returns the MAC address of the peripheral or empty string. The format is in
  // "00:B0:D0:63:C2:26".
  std::string GetAddress() const override;

  UniqueId GetUniqueId() const override { return mac_address_.address(); }

  bool Ok() const { return mac_address_.IsSet(); }
  explicit operator bool() const { return Ok(); }

 private:
  const MacAddress mac_address_;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_V2_PERIPHERAL_H_
