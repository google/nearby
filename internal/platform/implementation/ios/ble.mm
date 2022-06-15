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

#import "internal/platform/implementation/ios/ble.h"

#include <CoreBluetooth/CoreBluetooth.h>
#include <string>

#include "internal/platform/implementation/ble_v2.h"
#import "internal/platform/implementation/ios/Mediums/Ble/GNCMBleCentral.h"
#import "internal/platform/implementation/ios/Mediums/Ble/GNCMBlePeripheral.h"
#include "internal/platform/implementation/ios/bluetooth_adapter.h"
#include "internal/platform/implementation/ios/utils.h"

namespace location {
namespace nearby {
namespace ios {

namespace {

CBAttributePermissions PermissionToCBPermissions(
    const std::vector<api::ble_v2::GattCharacteristic::Permission>& permissions) {
  CBAttributePermissions characteristPermissions = 0;
  for (const auto& permission : permissions) {
    switch (permission) {
      case api::ble_v2::GattCharacteristic::Permission::kRead:
        characteristPermissions |= CBAttributePermissionsReadable;
        break;
      case api::ble_v2::GattCharacteristic::Permission::kWrite:
        characteristPermissions |= CBAttributePermissionsWriteable;
        break;
      case api::ble_v2::GattCharacteristic::Permission::kLast:
      case api::ble_v2::GattCharacteristic::Permission::kUnknown:
      default:;  // fall through
    }
  }
  return characteristPermissions;
}

CBCharacteristicProperties PropertiesToCBProperties(
    const std::vector<api::ble_v2::GattCharacteristic::Property>& properties) {
  CBCharacteristicProperties characteristicProperties = 0;
  for (const auto& property : properties) {
    switch (property) {
      case api::ble_v2::GattCharacteristic::Property::kRead:
        characteristicProperties |= CBCharacteristicPropertyRead;
        break;
      case api::ble_v2::GattCharacteristic::Property::kWrite:
        characteristicProperties |= CBCharacteristicPropertyWrite;
        break;
      case api::ble_v2::GattCharacteristic::Property::kIndicate:
        characteristicProperties |= CBCharacteristicPropertyIndicate;
        break;
      case api::ble_v2::GattCharacteristic::Property::kLast:
      case api::ble_v2::GattCharacteristic::Property::kUnknown:
      default:;  // fall through
    }
  }
  return characteristicProperties;
}

}  // namespace

using ::location::nearby::api::ble_v2::BleAdvertisementData;
using ::location::nearby::api::ble_v2::TxPowerLevel;
using ScanCallback = ::location::nearby::api::ble_v2::BleMedium::ScanCallback;

BleMedium::BleMedium(::location::nearby::api::BluetoothAdapter& adapter)
    : adapter_(static_cast<BluetoothAdapter*>(&adapter)) {}

bool BleMedium::StartAdvertising(
    const BleAdvertisementData& advertising_data,
    ::location::nearby::api::ble_v2::AdvertiseParameters advertise_set_parameters) {
  if (advertising_data.service_data.empty()) {
    return false;
  }
  const auto& service_uuid = advertising_data.service_data.begin()->first.Get16BitAsString();
  const ByteArray& service_data_bytes = advertising_data.service_data.begin()->second;

  if (!peripheral_) {
    peripheral_ = [[GNCMBlePeripheral alloc] init];
  }

  [peripheral_ startAdvertisingWithServiceUUID:ObjCStringFromCppString(service_uuid)
                             advertisementData:NSDataFromByteArray(service_data_bytes)];
  return true;
}

bool BleMedium::StopAdvertising() {
  peripheral_ = nil;
  return true;
}

bool BleMedium::StartScanning(const Uuid& service_uuid, TxPowerLevel tx_power_level,
                              ScanCallback scan_callback) {
  central_ = [[GNCMBleCentral alloc]
      initWithServiceUUID:ObjCStringFromCppString(service_uuid.Get16BitAsString())
        scanResultHandler:^(NSString* serviceUUID, NSData* serviceData){
            // TODO(b/228751356): Add scan callback implementation.
        }];

  return true;
}

bool BleMedium::StopScanning() {
  central_ = nil;
  return true;
}

std::unique_ptr<api::ble_v2::GattServer> BleMedium::StartGattServer(
    api::ble_v2::ServerGattConnectionCallback callback) {
  if (!peripheral_) {
    peripheral_ = [[GNCMBlePeripheral alloc] init];
  }
  return std::make_unique<GattServer>(peripheral_);
}

std::unique_ptr<api::ble_v2::GattClient> BleMedium::ConnectToGattServer(
    api::ble_v2::BlePeripheral& peripheral, TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  return nullptr;
}

std::unique_ptr<api::ble_v2::BleServerSocket> BleMedium::OpenServerSocket(
    const std::string& service_id) {
  return nullptr;
}

std::unique_ptr<api::ble_v2::BleSocket> BleMedium::Connect(const std::string& service_id,
                                                           TxPowerLevel tx_power_level,
                                                           api::ble_v2::BlePeripheral& peripheral,
                                                           CancellationFlag* cancellation_flag) {
  return nullptr;
}

bool BleMedium::IsExtendedAdvertisementsAvailable() { return false; }

// NOLINTNEXTLINE
absl::optional<api::ble_v2::GattCharacteristic> BleMedium::GattServer::CreateCharacteristic(
    const Uuid& service_uuid, const Uuid& characteristic_uuid,
    const std::vector<api::ble_v2::GattCharacteristic::Permission>& permissions,
    const std::vector<api::ble_v2::GattCharacteristic::Property>& properties) {
  api::ble_v2::GattCharacteristic characteristic = {.uuid = characteristic_uuid,
                                                    .service_uuid = service_uuid,
                                                    .permissions = permissions,
                                                    .properties = properties};
  [peripheral_
      addCBServiceWithUUID:[CBUUID
                               UUIDWithString:ObjCStringFromCppString(
                                                  characteristic.service_uuid.Get16BitAsString())]];
  [peripheral_
      addCharacteristic:[[CBMutableCharacteristic alloc]
                            initWithType:[CBUUID UUIDWithString:ObjCStringFromCppString(std::string(
                                                                    characteristic.uuid))]
                              properties:PropertiesToCBProperties(characteristic.properties)
                                   value:nil
                             permissions:PermissionToCBPermissions(characteristic.permissions)]];
  return characteristic;
}

bool BleMedium::GattServer::UpdateCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    const location::nearby::ByteArray& value) {
  [peripheral_ updateValue:NSDataFromByteArray(value)
         forCharacteristic:[CBUUID UUIDWithString:ObjCStringFromCppString(
                                                      std::string(characteristic.uuid))]];
  return true;
}

void BleMedium::GattServer::Stop() {
  [peripheral_ stopGATTService];
}

}  // namespace ios
}  // namespace nearby
}  // namespace location
