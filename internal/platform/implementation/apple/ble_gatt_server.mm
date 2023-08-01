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

#import "internal/platform/implementation/apple/ble_gatt_server.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include "internal/platform/implementation/ble_v2.h"

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/ble_utils.h"
#import "internal/platform/implementation/apple/utils.h"
#import "GoogleToolboxForMac/GTMLogger.h"

namespace nearby {
namespace apple {

GattServer::GattServer(GNCBLEGATTServer *gatt_server) : gatt_server_(gatt_server) {}

std::optional<api::ble_v2::GattCharacteristic> GattServer::CreateCharacteristic(
    const Uuid &service_uuid, const Uuid &characteristic_uuid,
    api::ble_v2::GattCharacteristic::Permission permission,
    api::ble_v2::GattCharacteristic::Property property) {
  CBUUID *serviceUUID = CBUUID128FromCPP(service_uuid);
  CBUUID *characteristicUUID = CBUUID128FromCPP(characteristic_uuid);
  CBAttributePermissions permissions = CBAttributePermissionsFromCPP(permission);
  CBCharacteristicProperties properties = CBCharacteristicPropertiesFromCPP(property);

  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];
  __block GNCBLEGATTCharacteristic *blockCharacteristic = nil;
  [gatt_server_ createCharacteristicWithServiceID:serviceUUID
                               characteristicUUID:characteristicUUID
                                      permissions:permissions
                                       properties:properties
                                completionHandler:^(GNCBLEGATTCharacteristic *characteristic,
                                                    NSError *error) {
                                  [condition lock];
                                  if (error != nil) {
                                    GTMLoggerError(@"Error creating characteristic: %@", error);
                                  }
                                  blockCharacteristic = characteristic;
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

bool GattServer::UpdateCharacteristic(const api::ble_v2::GattCharacteristic &characteristic,
                                      const nearby::ByteArray &value) {
  NSCondition *condition = [[NSCondition alloc] init];
  [condition lock];
  __block NSError *blockError = nil;
  [gatt_server_ updateCharacteristic:ObjCGATTCharacteristicFromCPP(characteristic)
                               value:NSDataFromByteArray(value)
                   completionHandler:^(NSError *error) {
                     [condition lock];
                     if (error != nil) {
                       GTMLoggerError(@"Error updating characteristic: %@", error);
                     }
                     blockError = error;
                     [condition signal];
                     [condition unlock];
                   }];
  [condition wait];
  [condition unlock];
  return blockError == nil;
}

// TODO(b/290385712): Implement.
absl::Status GattServer::NotifyCharacteristicChanged(
    const api::ble_v2::GattCharacteristic &characteristic, bool confirm,
    const ByteArray &new_value) {
  return absl::UnimplementedError("");
}

void GattServer::Stop() {
  [gatt_server_ stop];
}

// TODO(b/290385712): Implement.
api::ble_v2::BlePeripheral &GattServer::GetBlePeripheral() {
  return peripheral_;
}

}  // namespace apple
}  // namespace nearby
