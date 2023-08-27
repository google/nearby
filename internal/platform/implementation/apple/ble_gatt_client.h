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

#include <string>
#include <vector>

#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/uuid.h"

@class GNCBLEGATTClient;

namespace nearby {
namespace apple {

class GattClient : public api::ble_v2::GattClient {
 public:
  explicit GattClient(GNCBLEGATTClient *gatt_client_);
  ~GattClient() override = default;

  // Discovers the specified characteristics of a service.
  //
  // This method blocks until discovery has finished.
  //
  // Returns whether or not discovery finished successfully.
  bool DiscoverServiceAndCharacteristics(const Uuid &service_uuid,
                                         const std::vector<Uuid> &characteristic_uuids) override;

  // Retrieves a GATT characteristic.
  //
  // DiscoverServiceAndCharacteristics() must be called before this method to fetch all available
  // services and characteristics first.
  //
  // On success, returns the characteristic. On error, returns nullptr.
  std::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
      const Uuid &service_uuid, const Uuid &characteristic_uuid) override;

  // Reads the requested characteristic from the associated remote device.
  //
  // On success, returns the characteristic's value. On error, returns nullptr.
  std::optional<std::string> ReadCharacteristic(
      const api::ble_v2::GattCharacteristic &characteristic) override;

  // Writes a given characteristic and its values to the associated remote device.
  //
  // Returns whether or not the write was successful.
  bool WriteCharacteristic(const api::ble_v2::GattCharacteristic &characteristic,
                           absl::string_view value,
                           api::ble_v2::GattClient::WriteType type) override;

  // Enable or disable notifications/indications for a given characteristic.
  //
  // Once notifications are enabled for a characteristic, on_characteristic_changed_cb will be
  // triggered if the remote device indicates that the given characteristic has changed.
  //
  // Returns whether or not the subscription was successful.
  bool SetCharacteristicSubscription(
      const api::ble_v2::GattCharacteristic &characteristic, bool enable,
      absl::AnyInvocable<void(absl::string_view value)> on_characteristic_changed_cb) override;

  // Disconnects an established connection, or cancels a connection attempt currently in progress.
  void Disconnect() override;

 private:
  GNCBLEGATTClient *gatt_client_;
};

}  // namespace apple
}  // namespace nearby
