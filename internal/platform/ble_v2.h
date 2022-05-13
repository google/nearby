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

#ifndef PLATFORM_PUBLIC_BLE_V2_H_
#define PLATFORM_PUBLIC_BLE_V2_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/mutex.h"

namespace location {
namespace nearby {

// Opaque wrapper over a GattServer.
// Move only, disallow copy.
//
// Note that some of the methods return absl::optional instead
// of std::optional, because iOS platform is still in C++14.
class GattServer final {
 public:
  explicit GattServer(std::unique_ptr<api::ble_v2::GattServer> gatt_server)
      : impl_(std::move(gatt_server)) {}
  ~GattServer() { Stop(); }

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
      const std::string& service_uuid, const std::string& characteristic_uuid,
      const std::vector<api::ble_v2::GattCharacteristic::Permission>&
          permissions,
      const std::vector<api::ble_v2::GattCharacteristic::Property>&
          properties) {
    return impl_->CreateCharacteristic(service_uuid, characteristic_uuid,
                                       permissions, properties);
  }

  bool UpdateCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      const ByteArray& value) {
    return impl_->UpdateCharacteristic(characteristic, value);
  }

  void Stop() { return impl_->Stop(); }

  // Returns true if a gatt_server is usable. If this method returns false,
  // it is not safe to call any other method.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  api::ble_v2::GattServer* GetImpl() { return impl_.get(); }

 private:
  std::unique_ptr<api::ble_v2::GattServer> impl_;
};

// Opaque wrapper for a GattClient.
//
// Note that some of the methods return absl::optional instead
// of std::optional, because iOS platform is still in C++14.
class GattClient final {
 public:
  explicit GattClient(
      std::unique_ptr<api::ble_v2::GattClient> client_gatt_connection)
      : impl_(std::move(client_gatt_connection)) {}

  bool DiscoverService(const std::string& service_uuid) {
    return impl_->DiscoverService(service_uuid);
  }

  // TODO(edwinwu): Change std::string to Uuid.
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
      const std::string& service_uuid, const std::string& characteristic_uuid) {
    return impl_->GetCharacteristic(service_uuid, characteristic_uuid);
  }

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<ByteArray> ReadCharacteristic(
      api::ble_v2::GattCharacteristic& characteristic) {
    return impl_->ReadCharacteristic(characteristic);
  }

  void Disconnect() { impl_->Disconnect(); }

  // Returns true if a client_gatt_connection is usable. If this method returns
  // false, it is not safe to call any other method.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  api::ble_v2::GattClient* GetImpl() { return impl_.get(); }

 private:
  std::unique_ptr<api::ble_v2::GattClient> impl_;
};

// Container of operations that can be performed over the BLE medium.
class BleV2Medium final {
 public:
  // A wrapper callback for BLE scan results.
  //
  // The peripheral is a wrapper object which stores the real impl of
  // api::BlePeripheral.
  // The reference will remain valid while api::BlePeripheral object is
  // itself valid. Typically peripheral lifetime matches duration of the
  // connection, and is controlled by primitive client, since they hold the
  // instance.
  struct ScanCallback {
    std::function<void(
        BleV2Peripheral peripheral,
        const api::ble_v2::BleAdvertisementData& advertisement_data)>
        advertisement_found_cb = location::nearby::DefaultCallback<
            BleV2Peripheral, const api::ble_v2::BleAdvertisementData&>();
  };
  struct ServerGattConnectionCallback {
    std::function<void(const api::ble_v2::GattCharacteristic& characteristic)>
        characteristic_subscription_cb = location::nearby::DefaultCallback<
            const api::ble_v2::GattCharacteristic&>();
    std::function<void(const api::ble_v2::GattCharacteristic& characteristic)>
        characteristic_unsubscription_cb = location::nearby::DefaultCallback<
            const api::ble_v2::GattCharacteristic&>();
  };
  // TODO(b/231318879): Remove this wrapper callback and use impl callback if
  // there is only disconnect function here in the end.
  struct ClientGattConnectionCallback {
    std::function<void()> disconnected_cb =
        location::nearby::DefaultCallback<>();
  };

  explicit BleV2Medium(BluetoothAdapter& adapter)
      : impl_(
            api::ImplementationPlatform::CreateBleV2Medium(adapter.GetImpl())),
        adapter_(adapter) {}

  // Returns true once the BLE advertising has been initiated.
  bool StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      const api::ble_v2::BleAdvertisementData& scan_response_data,
      api::ble_v2::PowerMode power_mode);
  bool StopAdvertising();

  // Returns true once the BLE scan has been initiated.
  bool StartScanning(const std::string& service_uuid,
                     api::ble_v2::PowerMode power_mode, ScanCallback callback);
  bool StopScanning();

  // Starts Gatt Server for waiting to client connection.
  std::unique_ptr<GattServer> StartGattServer(
      ServerGattConnectionCallback callback);

  // Returns a new GattClient connection to a gatt server.
  // There is only one instance of GattServer can run at a time.
  std::unique_ptr<GattClient> ConnectToGattServer(
      BleV2Peripheral peripheral, api::ble_v2::PowerMode power_mode,
      ClientGattConnectionCallback callback);

  bool IsValid() const { return impl_ != nullptr; }

  api::ble_v2::BleMedium* GetImpl() const { return impl_.get(); }

 private:
  Mutex mutex_;
  std::unique_ptr<api::ble_v2::BleMedium> impl_;
  BluetoothAdapter& adapter_;
  ServerGattConnectionCallback server_gatt_connection_callback_
      ABSL_GUARDED_BY(mutex_);
  ClientGattConnectionCallback client_gatt_connection_callback_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<api::ble_v2::BlePeripheral*> peripherals_
      ABSL_GUARDED_BY(mutex_);
  ScanCallback scan_callback_ ABSL_GUARDED_BY(mutex_);
  bool scanning_enabled_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_BLE_V2_H_
