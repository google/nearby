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

// Note: File language is detected using heuristics. Many Objective-C++ headers are incorrectly
// classified as C++ resulting in invalid linter errors. The use of "NSArray" and other Foundation
// classes like "NSData", "NSDictionary" and "NSUUID" are highly weighted for Objective-C and
// Objective-C++ scores. Oddly, "#import <Foundation/Foundation.h>" does not contribute any points.
// This comment alone should be enough to trick the IDE in to believing this is actually some sort
// of Objective-C file. See: cs/google3/devtools/search/lang/recognize_language_classifiers_data

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include <string>

#include "internal/platform/implementation/ble_v2.h"

@protocol GNCPeripheral;

namespace nearby {
namespace apple {

// An empty peripheral.
//
// Apple APIs do not expose a peripheral's MAC address and does not provide a
// way to directly connect to a given MAC address. Instead a connection can only
// be made using a CoreBluetooth peripheral object. Many times a CoreBluetooth
// peripheral is not available, namely, when the remote device is a central. For
// these cases, an EmptyBlePeripheral should be used.
class EmptyBlePeripheral : public api::ble_v2::BlePeripheral {
 public:
  EmptyBlePeripheral();
  ~EmptyBlePeripheral() override = default;

  // Returns an empty string.
  std::string GetAddress() const override;

  // Returns an immutable unique identifier. The identifier does not change when
  // the peripheral's address is rotated.
  api::ble_v2::BlePeripheral::UniqueId GetUniqueId() const override;

 private:
  api::ble_v2::BlePeripheral::UniqueId unique_id_;
};

// A wrapper of a CoreBluetooth peripheral object. This can be used to uniquely
// identify a peripheral and connect to its GATT server.
//
// Many times a CoreBluetooth peripheral is not available, namely, when the
// remote device is a central. For these cases, an EmptyBlePeripheral should be
// used instead.
class BlePeripheral : public api::ble_v2::BlePeripheral {
 public:
  explicit BlePeripheral(id<GNCPeripheral> peripheral);
  ~BlePeripheral() override = default;

  // Returns an empty string.
  std::string GetAddress() const override;

  // Returns an immutable unique identifier. The identifier does not change when
  // the peripheral's address is rotated.
  api::ble_v2::BlePeripheral::UniqueId GetUniqueId() const override;

  // Returns the CoreBluetooth peripheral object.
  id<GNCPeripheral> GetPeripheral() const;

 private:
  id<GNCPeripheral> peripheral_;
  api::ble_v2::BlePeripheral::UniqueId unique_id_;
};

}  // namespace apple
}  // namespace nearby
