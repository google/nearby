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

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "third_party/nearby/cpp/cal/api/ble.h"
#include "third_party/nearby/cpp/cal/base/ble_types.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/mutex.h"
#include "internal/platform/output_stream.h"

namespace location {
namespace nearby {

// Opaque wrapper over a BLE peripheral. Must contain enough data about a
// particular BLE peripheral to connect to its GATT server.
class BlePeripheralV2 final {
 public:
  BlePeripheralV2() = default;
  BlePeripheralV2(const BlePeripheralV2&) = default;
  BlePeripheralV2& operator=(const BlePeripheralV2&) = default;
  explicit BlePeripheralV2(::nearby::cal::api::BlePeripheral* peripheral)
      : impl_(peripheral) {}
  ~BlePeripheralV2() = default;

  std::string GetName() const { return impl_->GetName(); }

  std::string GetAddress() const { return impl_->GetAddress(); }

  location::nearby::ByteArray GetAdvertisementBytes() const {
    return impl_->GetAdvertisementBytes();
  }

  ::nearby::cal::api::BlePeripheral& GetImpl() { return *impl_; }
  bool IsValid() const { return impl_ != nullptr; }

 private:
  ::nearby::cal::api::BlePeripheral* impl_;
};

// Opaque wrapper over a BLE Gatt Characteristic.
class GattCharacteristic final {
 public:
  GattCharacteristic() = default;
  GattCharacteristic(const GattCharacteristic&) = default;
  GattCharacteristic& operator=(const GattCharacteristic&) = default;
  explicit GattCharacteristic(
      ::nearby::cal::api::GattCharacteristic* characteristic)
      : impl_(characteristic) {}
  explicit GattCharacteristic(
      std::unique_ptr<::nearby::cal::api::GattCharacteristic> characteristic)
      : impl_(characteristic.release()) {}
  ~GattCharacteristic() = default;

  std::string GetUuid() const { return impl_->GetUuid(); }

  std::string GetServiceUuid() const { return impl_->GetServiceUuid(); }

  ::nearby::cal::api::GattCharacteristic& GetImpl() { return *impl_; }
  bool IsValid() const { return impl_ != nullptr; }

 private:
  ::nearby::cal::api::GattCharacteristic* impl_;
};

class GattServer final {
 public:
  GattServer() = default;
  GattServer(const GattServer&) = default;
  GattServer& operator=(const GattServer&) = default;
  explicit GattServer(::nearby::cal::api::GattServer* gatt_server)
      : impl_(gatt_server) {}
  explicit GattServer(
      std::unique_ptr<::nearby::cal::api::GattServer> gatt_server)
      : impl_(gatt_server.release()) {}
  ~GattServer() = default;

  GattCharacteristic CreateCharacteristic(
      const std::string& service_uuid, const std::string& characteristic_uuid,
      const std::vector<::nearby::cal::api::GattCharacteristic::Permission>&
          permissions,
      const std::vector<::nearby::cal::api::GattCharacteristic::Property>&
          properties) {
    return GattCharacteristic(impl_->CreateCharacteristic(
        service_uuid, characteristic_uuid, permissions, properties));
  }

  bool UpdateCharacteristic(GattCharacteristic& characteristic,
                            const ByteArray& value) {
    return impl_->UpdateCharacteristic(characteristic.GetImpl(), value);
  }

  void Stop() { return impl_->Stop(); }

  // Returns true if a gatt_server is usable. If this method returns false,
  // it is not safe to call any other method.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  ::nearby::cal::api::GattServer& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<::nearby::cal::api::GattServer> impl_;
};

class ClientGattConnection final {
 public:
  ClientGattConnection() = default;
  ClientGattConnection(const ClientGattConnection&) = default;
  ClientGattConnection& operator=(const ClientGattConnection&) = default;
  explicit ClientGattConnection(
      ::nearby::cal::api::ClientGattConnection* client_gatt_connection)
      : impl_(client_gatt_connection) {}
  explicit ClientGattConnection(
      std::unique_ptr<::nearby::cal::api::ClientGattConnection>
          client_gatt_connection)
      : impl_(client_gatt_connection.release()) {}
  ~ClientGattConnection() = default;

  BlePeripheralV2 GetPeripheral() {
    return BlePeripheralV2(&impl_->GetPeripheral());
  }

  bool DiscoverServices() { return impl_->DiscoverServices(); }

  GattCharacteristic GetCharacteristic(const std::string& service_uuid,
                                       const std::string& characteristic_uuid) {
    return GattCharacteristic(
        impl_->GetCharacteristic(service_uuid, characteristic_uuid));
  }

  absl::optional<location::nearby::ByteArray> ReadCharacteristic(
      GattCharacteristic& characteristic) {
    return impl_->ReadCharacteristic(characteristic.GetImpl());
  }

  bool WriteCharacteristic(GattCharacteristic& characteristic,
                           const location::nearby::ByteArray& value) {
    return impl_->WriteCharacteristic(characteristic.GetImpl(), value);
  }

  void Disconnect() { impl_->Disconnect(); }

  // Returns true if a client_gatt_connection is usable. If this method returns
  // false, it is not safe to call any other method.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  ::nearby::cal::api::ClientGattConnection& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<::nearby::cal::api::ClientGattConnection> impl_;
};

// Container of operations that can be performed over the BLE medium.
class BleV2Medium final {
 public:
  using Platform = api::ImplementationPlatform;

  struct ScanCallback {
    std::function<void(
        const ::nearby::cal::BleAdvertisementData& advertisement_data)>
        advertisement_found_cb = location::nearby::DefaultCallback<
            const ::nearby::cal::BleAdvertisementData&>();
  };

  struct ScanningInfo {
    BlePeripheralV2 peripheral;
  };

  struct ServerGattConnectionCallback {
    std::function<void(
        const ::nearby::cal::api::ServerGattConnection& connection,
        GattCharacteristic characteristic)>
        characteristic_subscription_cb = location::nearby::DefaultCallback<
            const ::nearby::cal::api::ServerGattConnection&,
            GattCharacteristic>();
    std::function<void(
        const ::nearby::cal::api::ServerGattConnection& connection,
        GattCharacteristic characteristic)>
        characteristic_unsubscription_cb = location::nearby::DefaultCallback<
            const ::nearby::cal::api::ServerGattConnection&,
            GattCharacteristic>();
  };

  struct ClientGattConnectionCallback {
   public:
    std::function<void(ClientGattConnection& connection)> disconnected_cb =
        location::nearby::DefaultCallback<ClientGattConnection&>();
  };

  explicit BleV2Medium(BluetoothAdapter& adapter)
      : impl_(Platform::CreateBleV2Medium(adapter.GetImpl())),
        adapter_(adapter) {}

  // Returns true once the BLE advertising has been initiated.
  bool StartAdvertising(
      const ::nearby::cal::BleAdvertisementData& advertising_data,
      const ::nearby::cal::BleAdvertisementData& scan_response_data,
      ::nearby::cal::PowerMode power_mode);
  bool StopAdvertising();

  // Returns true once the BLE scan has been initiated.
  bool StartScanning(const std::vector<std::string>& service_uuids,
                     ::nearby::cal::PowerMode power_mode,
                     ScanCallback callback);
  bool StopScanning();

  GattServer StartGattServer(ServerGattConnectionCallback callback);

  // Returns a new BleSocket. On Success, BleSocket::IsValid()
  // returns true.
  ClientGattConnection ConnectToGattServer(
      BlePeripheralV2& peripheral, int mtu, ::nearby::cal::PowerMode power_mode,
      ClientGattConnectionCallback callback);
  bool IsValid() const { return impl_ != nullptr; }

  ::nearby::cal::api::BleMedium& GetImpl() { return *impl_; }
  BluetoothAdapter& GetAdapter() { return adapter_; }

 private:
  Mutex mutex_;
  std::unique_ptr<::nearby::cal::api::BleMedium> impl_;
  BluetoothAdapter& adapter_;
  ServerGattConnectionCallback server_gatt_connection_callback_
      ABSL_GUARDED_BY(mutex_);
  ClientGattConnectionCallback client_gatt_connection_callback_
      ABSL_GUARDED_BY(mutex_);
  ScanCallback scan_callback_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_BLE_V2_H_
