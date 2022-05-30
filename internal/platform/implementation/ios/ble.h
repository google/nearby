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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_IOS_BLE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_IOS_BLE_H_

#import <Foundation/Foundation.h>

#include <string>

#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/ios/bluetooth_adapter.h"

@class GNCMBlePeripheral, GNCMBleCentral;

namespace location {
namespace nearby {
namespace ios {

/** Concrete BleMedium implementation. */
class BleMedium : public api::ble_v2::BleMedium {
 public:
  explicit BleMedium(api::BluetoothAdapter& adapter);

  // api::BleMedium:
  bool StartAdvertising(const api::ble_v2::BleAdvertisementData& advertising_data,
                        api::ble_v2::AdvertiseParameters advertise_set_parameters) override;
  bool StopAdvertising() override;
  bool StartScanning(const Uuid& service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
                     api::ble_v2::BleMedium::ScanCallback scan_callback) override;
  bool StopScanning() override;
  std::unique_ptr<api::ble_v2::GattServer> StartGattServer(
      api::ble_v2::ServerGattConnectionCallback callback) override;
  std::unique_ptr<api::ble_v2::GattClient> ConnectToGattServer(
      api::ble_v2::BlePeripheral& peripheral, api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::ClientGattConnectionCallback callback) override;
  std::unique_ptr<api::ble_v2::BleServerSocket> OpenServerSocket(
      const std::string& service_id) override;
  std::unique_ptr<api::ble_v2::BleSocket> Connect(const std::string& service_id,
                                                  api::ble_v2::TxPowerLevel tx_power_level,
                                                  api::ble_v2::BlePeripheral& peripheral,
                                                  CancellationFlag* cancellation_flag) override;
  bool IsExtendedAdvertisementsAvailable() override;

 private:
  BluetoothAdapter* adapter_;
  GNCMBlePeripheral* peripheral_;
  GNCMBleCentral* central_;
  dispatch_queue_t callback_queue_;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_IOS_BLE_H_
