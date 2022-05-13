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

#ifndef PLATFORM_IMPL_G3_BLE_V2_H_
#define PLATFORM_IMPL_G3_BLE_V2_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/g3/bluetooth_adapter.h"

namespace location {
namespace nearby {
namespace g3 {

// TODO(b/213691253): Add g3 BleV2 medium tests after more functions are ready.
// Container of operations that can be performed over the BLE medium.
class BleV2Medium : public api::ble_v2::BleMedium {
 public:
  explicit BleV2Medium(api::BluetoothAdapter& adapter);
  ~BleV2Medium() override;

  // Returns true once the Ble advertising has been initiated.
  bool StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      const api::ble_v2::BleAdvertisementData& scan_response_data,
      api::ble_v2::PowerMode power_mode) override ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAdvertising() override ABSL_LOCKS_EXCLUDED(mutex_);

  bool StartScanning(const std::string& service_uuid,
                     api::ble_v2::PowerMode power_mode,
                     ScanCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopScanning() override ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble_v2::GattServer> StartGattServer(
      api::ble_v2::ServerGattConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StartListeningForIncomingBleSockets(
      const api::ble_v2::ServerBleSocketLifeCycleCallback& callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void StopListeningForIncomingBleSockets() override
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble_v2::GattClient> ConnectToGattServer(
      api::ble_v2::BlePeripheral& peripheral, api::ble_v2::PowerMode power_mode,
      api::ble_v2::ClientGattConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble_v2::BleSocket> EstablishBleSocket(
      api::ble_v2::BlePeripheral* peripheral,
      const api::ble_v2::BleSocketLifeCycleCallback& callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  BluetoothAdapter& GetAdapter() { return *adapter_; }

 private:
  // A concrete implemenation for GattServer.
  class GattServer : public api::ble_v2::GattServer {
   public:
    std::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
        absl::string_view service_uuid, absl::string_view characteristic_uuid,
        const std::vector<api::ble_v2::GattCharacteristic::Permission>&
            permissions,
        const std::vector<api::ble_v2::GattCharacteristic::Property>&
            properties) override;

    bool UpdateCharacteristic(
        const api::ble_v2::GattCharacteristic& characteristic,
        const location::nearby::ByteArray& value) override;

    void Stop() override;
  };

  // A concrete implemenation for GattClient.
  class GattClient : public api::ble_v2::GattClient {
   public:
    bool DiscoverService(const std::string& service_uuid) override;

    std::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
        absl::string_view service_uuid,
        absl::string_view characteristic_uuid) override;

    std::optional<ByteArray> ReadCharacteristic(
        const api::ble_v2::GattCharacteristic& characteristic) override;

    bool WriteCharacteristic(
        const api::ble_v2::GattCharacteristic& characteristic,
        const ByteArray& value) override;

    void Disconnect() override;

   private:
    absl::Mutex mutex_;

    // A flag to indicate the gatt connection alive or not. If it is
    // disconnected/*false*/, the instance needs to be created again to bring it
    // alive.
    bool is_connection_alive_ ABSL_GUARDED_BY(mutex_) = true;
  };

  absl::Mutex mutex_;
  BluetoothAdapter* adapter_;  // Our device adapter; read-only.
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_G3_BLE_V2_H_
