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

#import <Foundation/Foundation.h>

#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/uuid.h"

#import "internal/platform/implementation/apple/ble_peripheral.h"

@class GNCBLEGATTServer;

namespace nearby {
namespace apple {

class GattServer : public api::ble_v2::GattServer {
 public:
  explicit GattServer(GNCBLEGATTServer *gatt_server_);
  ~GattServer() override = default;

  // Returns an empty BlePeripheral object.
  //
  // Use of this method should be avoided and its only purpose seems to be a check that the GATT
  // server is valid.
  api::ble_v2::BlePeripheral &GetBlePeripheral() override;

  // Creates a characteristic and adds it to the GATT server under the given characteristic and
  // service UUIDs.
  //
  // Characteristics of the same service UUID will be put under one service rather than many
  // services with the same UUID.
  //
  // Returns no value upon error.
  std::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
      const Uuid &service_uuid, const Uuid &characteristic_uuid,
      api::ble_v2::GattCharacteristic::Permission permission,
      api::ble_v2::GattCharacteristic::Property property) override;

  // Updates a local characteristic with the provided value.
  //
  // Returns whether or not the update was successful.
  bool UpdateCharacteristic(const api::ble_v2::GattCharacteristic &characteristic,
                            const nearby::ByteArray &value) override;

  // Send a notification or indication that a local characteristic has been updated.
  //
  // Returns an absl::Status indicating success or what went wrong.
  absl::Status NotifyCharacteristicChanged(const api::ble_v2::GattCharacteristic &characteristic,
                                           bool confirm, const ByteArray &new_value) override;

  // Stops a GATT server.
  void Stop() override;

 private:
  GNCBLEGATTServer *gatt_server_;
  EmptyBlePeripheral peripheral_;
};

}  // namespace apple
}  // namespace nearby
