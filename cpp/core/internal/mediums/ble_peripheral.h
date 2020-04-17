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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_PERIPHERAL_H_
#define CORE_INTERNAL_MEDIUMS_BLE_PERIPHERAL_H_

#include "platform/byte_array.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

class BLEPeripheral {
 public:
  explicit BLEPeripheral(ConstPtr<ByteArray> id);
  ~BLEPeripheral();

  ConstPtr<ByteArray> getId() const;

 private:
  // A unique identifier for this peripheral. It can be the BLE advertisement it
  // was found on, or even simply the BLE MAC address.
  ScopedPtr<ConstPtr<ByteArray>> id_;
};


// Represents BLE peripheral for testing.
class BlePeripheral {
 public:
  explicit BlePeripheral(const ByteArray& id) : id_(id) {}
  ~BlePeripheral() = default;

  const ByteArray& GetId() const { return id_; }

 private:
  // A unique identifier for this peripheral. It can be the BLE advertisement it
  // was found on, or even simply the BLE MAC address.
  const ByteArray id_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_BLE_PERIPHERAL_H_
