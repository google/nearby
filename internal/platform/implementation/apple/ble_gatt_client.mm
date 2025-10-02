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

#import "internal/platform/implementation/apple/ble_gatt_client.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include <string>
#include <utility>
#include <vector>

#include "internal/platform/implementation/ble.h"

#import "internal/platform/implementation/apple/Flags/GNCFeatureFlags.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTClient.h"
#import "internal/platform/implementation/apple/ble_utils.h"

namespace nearby {
namespace apple {

GattClient::GattClient(GNCBLEGATTClient *gatt_client) : gatt_client_(gatt_client) {}

bool GattClient::DiscoverServiceAndCharacteristics(const Uuid &service_uuid,
                                                   const std::vector<Uuid> &characteristic_uuids) {
  CBUUID *serviceUUID = CBUUID128FromCPP(service_uuid);
  NSMutableArray<CBUUID *> *characteristics = [[NSMutableArray alloc] init];
  for (const auto &uuid : characteristic_uuids) {
    [characteristics addObject:CBUUID128FromCPP(uuid)];
  }

  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];
  __block NSError *blockError = nil;
  [gatt_client_ discoverCharacteristicsWithUUIDs:characteristics
                                     serviceUUID:serviceUUID
                               completionHandler:^(NSError *error) {
                                 [condition lock];
                                 if (error != nil) {
                                   GNCLoggerError(@"Error discovering characteristics: %@", error);
                                 }
                                 blockError = error;
                                 [condition signal];
                                 [condition unlock];
                               }];
  [condition wait];
  [condition unlock];
  return blockError == nil;
}

std::optional<api::ble::GattCharacteristic> GattClient::GetCharacteristic(
    const Uuid &service_uuid, const Uuid &characteristic_uuid) {
  CBUUID *serviceUUID = CBUUID128FromCPP(service_uuid);
  CBUUID *characteristicUUID = CBUUID128FromCPP(characteristic_uuid);

  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];
  __block GNCBLEGATTCharacteristic *blockCharacteristic = nil;
  __block NSError *blockError = nil;
  [gatt_client_ characteristicWithUUID:characteristicUUID
                           serviceUUID:serviceUUID
                     completionHandler:^(GNCBLEGATTCharacteristic *characteristic, NSError *error) {
                       [condition lock];
                       if (error != nil) {
                         GNCLoggerError(@"Error retrieving characteristic: %@", error);
                       }
                       blockCharacteristic = characteristic;
                       blockError = error;
                       [condition signal];
                       [condition unlock];
                     }];
  [condition wait];
  [condition unlock];
  if (blockCharacteristic == nil) {
    return std::nullopt;
  }
  return CPPGATTCharacteristicFromObjC(blockCharacteristic);
}

std::optional<std::string> GattClient::ReadCharacteristic(
    const api::ble::GattCharacteristic &characteristic) {
  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];
  __block NSData *blockValue = nil;
  __block NSError *blockError = nil;
  [gatt_client_ readValueForCharacteristic:ObjCGATTCharacteristicFromCPP(characteristic)
                         completionHandler:^(NSData *value, NSError *error) {
                           [condition lock];
                           if (error != nil) {
                             GNCLoggerError(@"Error reading characteristic: %@", error);
                           }
                           blockValue = value;
                           blockError = error;
                           [condition signal];
                           [condition unlock];
                         }];
  [condition wait];
  [condition unlock];
  if (blockValue == nil) {
    return std::nullopt;
  }
  return std::string((const char *)blockValue.bytes, blockValue.length);
}

// TODO(b/290385712): Implement.
bool GattClient::WriteCharacteristic(const api::ble::GattCharacteristic &characteristic,
                                     absl::string_view value,
                                     api::ble::GattClient::WriteType type) {
  return false;
}

// TODO(b/290385712): Implement.
bool GattClient::SetCharacteristicSubscription(
    const api::ble::GattCharacteristic &characteristic, bool enable,
    absl::AnyInvocable<void(absl::string_view value)> on_characteristic_changed_cb) {
  return false;
}

void GattClient::Disconnect() {
  // There seems to be an issue between some iOS<>Android device pairs where the Android device will
  // not connect to the iOS device if the iOS device disconnects and then attempts to reconnect.
  // Because of this, we no-op here instead of calling `[gatt_client_ disconnect]`.
  // See: b/375176623
  if (GNCFeatureFlags.gattClientDisconnectionEnabled) {
    // Avoid to impact GTV functionality, so that GATT client can reconnect to the GATT server.
    [gatt_client_ disconnect];
  }
}

}  // namespace apple
}  // namespace nearby
