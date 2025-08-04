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

#import "internal/platform/implementation/apple/ble_peripheral.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include <string>

#include "internal/platform/implementation/ble_v2.h"

#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"

namespace nearby {
namespace apple {

#pragma mark - BlePeripheral

BlePeripheral::BlePeripheral(id<GNCPeripheral> peripheral)
    : api::ble_v2::BlePeripheral(peripheral.identifier.hash), peripheral_(peripheral) {}

api::ble_v2::BlePeripheral& BlePeripheral::DefaultBlePeripheral() {
  static api::ble_v2::BlePeripheral* default_peripheral =
      new api::ble_v2::BlePeripheral(0xffffffffffffffff);
  return *default_peripheral;
}

id<GNCPeripheral> BlePeripheral::GetPeripheral() const { return peripheral_; }

}  // namespace apple
}  // namespace nearby
