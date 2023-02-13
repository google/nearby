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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_PERIPHERAL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_PERIPHERAL_H_

#include <string>

#include "internal/platform/implementation/ble.h"

namespace nearby {
namespace windows {

// TODO(b/269152309): Implement BLE Peripheral
// This is just a fake stub to appease the BLE Medium abstraction. Windows
// does not support BLE GATT-based advertising/discovery and socket currently,
// so a BlePeripheral is not required. The remote device is recognized as a
// BluetoothClassicDevice instead.

// Opaque wrapper over a BLE peripheral. Must contain enough data about a
// particular BLE device to connect to its GATT server.
class BlePeripheral : public api::BlePeripheral {
 public:
  ~BlePeripheral() override = default;

  std::string GetName() const override { return name_; }

  ByteArray GetAdvertisementBytes(
      const std::string& service_id) const override {
    return advertisement_data_;
  }

  void SetName(const std::string& name) {
    name_ = name;
  }

  void SetAdvertisementBytes(ByteArray advertisement_bytes) {
    advertisement_data_ = advertisement_bytes;
  }

 private:
  std::string name_;
  ByteArray advertisement_data_;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_BLE_PERIPHERAL_H_
