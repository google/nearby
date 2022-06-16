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

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include <string>

#include "absl/types/optional.h"
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
  // A concrete implemenation for GattServer.
  class GattServer : public api::ble_v2::GattServer {
   public:
    GattServer() = default;
    explicit GattServer(GNCMBlePeripheral* peripheral) : peripheral_(peripheral) {}

    absl::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
        const Uuid& service_uuid, const Uuid& characteristic_uuid,
        const std::vector<api::ble_v2::GattCharacteristic::Permission>& permissions,
        const std::vector<api::ble_v2::GattCharacteristic::Property>& properties) override;

    bool UpdateCharacteristic(const api::ble_v2::GattCharacteristic& characteristic,
                              const location::nearby::ByteArray& value) override;
    void Stop() override;

   private:
    GNCMBlePeripheral* peripheral_;
  };

  // A concrete implemenation for GattClient.
  class GattClient : public api::ble_v2::GattClient {
   public:
    GattClient() = default;
    explicit GattClient(GNCMBleCentral* central, const std::string& peripheral_id)
        : central_(central), peripheral_id_(peripheral_id) {}

    bool DiscoverServiceAndCharacteristics(const Uuid& service_uuid,
                                           const std::vector<Uuid>& characteristic_uuids) override;

    // NOLINTNEXTLINE
    absl::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
        const Uuid& service_uuid, const Uuid& characteristic_uuid) override;

    // NOLINTNEXTLINE
    absl::optional<ByteArray> ReadCharacteristic(
        const api::ble_v2::GattCharacteristic& characteristic) override;

    bool WriteCharacteristic(const api::ble_v2::GattCharacteristic& characteristic,
                             const ByteArray& value) override;

    void Disconnect() override;

   private:
    GNCMBleCentral* central_;
    std::string peripheral_id_;
    absl::flat_hash_map<api::ble_v2::GattCharacteristic, ByteArray> gatt_characteristic_values_;
  };

  BluetoothAdapter* adapter_;
  GNCMBlePeripheral* peripheral_;
  GNCMBleCentral* central_;
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_IOS_BLE_H_
