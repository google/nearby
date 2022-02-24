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

#ifndef PLATFORM_PUBLIC_BLE_V2_BLE_PERIPHERAL_H_
#define PLATFORM_PUBLIC_BLE_V2_BLE_PERIPHERAL_H_

#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"

namespace location {
namespace nearby {

// Opaque wrapper over a BLE device.
//
// BlePeripheral is a proxy object created as a result of Ble peripheral
// discovery. It is moveable only class, which means you need to std::move()
// when it is transferred.
// Its lifetime spans between calls to peripheral_discovered_cb and
// peripheral_lost_cb.
class BleV2Peripheral final {
 public:
  BleV2Peripheral() = default;
  // TODO(edwinwu): The movable class should be tested in /core BleEndPoint.
  BleV2Peripheral(BleV2Peripheral&& other) {
    impl_ = other.impl_;
    advertisement_bytes_ = std::move(other.advertisement_bytes_);
    other.impl_ = nullptr;
  }
  BleV2Peripheral& operator=(BleV2Peripheral&& other) {
    if (this != &other) {
      impl_ = other.impl_;
      advertisement_bytes_ = std::move(other.advertisement_bytes_);
      other.impl_ = nullptr;
    }
    return *this;
  }
  explicit BleV2Peripheral(api::ble_v2::BlePeripheral* peripheral)
      : impl_(peripheral) {}

  std::string GetId() const { return impl_->GetId(); }

  void SetName(const ByteArray& advertisement_bytes) {
    advertisement_bytes_ = advertisement_bytes;
  }

  const ByteArray& GetName() const { return advertisement_bytes_; }

  api::ble_v2::BlePeripheral* GetImpl() { return impl_; }

  bool IsValid() const { return impl_ != nullptr; }

 private:
  api::ble_v2::BlePeripheral* impl_;
  ByteArray advertisement_bytes_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_BLE_V2_BLE_PERIPHERAL_H_
