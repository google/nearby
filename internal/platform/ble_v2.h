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
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/mutex.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/logging.h"

namespace location {
namespace nearby {

// Opaque wrapper over a ServerGattConnection.
class ServerGattConnection final {
 public:
  ServerGattConnection() = default;
  explicit ServerGattConnection(
      api::ble_v2::ServerGattConnection* server_gatt_connection)
      : impl_(server_gatt_connection) {}

  bool SendCharacteristic(const api::ble_v2::GattCharacteristic& characteristic,
                          const ByteArray& value) {
    return impl_->SendCharacteristic(characteristic, value);
  }

  api::ble_v2::ServerGattConnection* GetImpl() { return impl_; }

  bool IsValid() const { return impl_ != nullptr; }

 private:
  api::ble_v2::ServerGattConnection* impl_ = nullptr;
};

// Opaque wrapper over a GattServer.
// Move only, disallow copy.
class GattServer final {
 public:
  GattServer() = default;
  explicit GattServer(std::unique_ptr<api::ble_v2::GattServer> gatt_server)
      : impl_(std::move(gatt_server)) {}
  GattServer(GattServer&&) = default;
  GattServer& operator=(GattServer&&) = default;
  ~GattServer() { Stop(); }

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

class ClientGattConnection final {
 public:
  ClientGattConnection() = default;
  ClientGattConnection(const ClientGattConnection&) = default;
  ClientGattConnection& operator=(const ClientGattConnection&) = default;
  explicit ClientGattConnection(
      api::ble_v2::ClientGattConnection* client_gatt_connection)
      : impl_(client_gatt_connection) {}
  explicit ClientGattConnection(
      std::unique_ptr<api::ble_v2::ClientGattConnection> client_gatt_connection)
      : impl_(client_gatt_connection.release()) {}
  ~ClientGattConnection() = default;

  BleV2Peripheral GetPeripheral() {
    return BleV2Peripheral(&impl_->GetPeripheral());
  }

  bool DiscoverServices() { return impl_->DiscoverServices(); }

  absl::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
      const std::string& service_uuid, const std::string& characteristic_uuid) {
    return impl_->GetCharacteristic(service_uuid, characteristic_uuid);
  }

  absl::optional<ByteArray> ReadCharacteristic(
      api::ble_v2::GattCharacteristic& characteristic) {
    return impl_->ReadCharacteristic(characteristic);
  }

  bool WriteCharacteristic(api::ble_v2::GattCharacteristic& characteristic,
                           const ByteArray& value) {
    return impl_->WriteCharacteristic(characteristic, value);
  }

  void Disconnect() { impl_->Disconnect(); }

  // Returns true if a client_gatt_connection is usable. If this method returns
  // false, it is not safe to call any other method.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  api::ble_v2::ClientGattConnection& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::ble_v2::ClientGattConnection> impl_;
};

class BleSocket final {
 public:
  BleSocket() = default;
  explicit BleSocket(std::unique_ptr<api::ble_v2::BleSocket> socket)
      : impl_(std::move(socket)) {}
  BleSocket(const BleSocket&) = default;
  BleSocket& operator=(const BleSocket&) = default;

  // Returns the InputStream of the BleSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleSocket object is destroyed.
  InputStream& GetInputStream() { return impl_->GetInputStream(); }

  // Returns the OutputStream of the BleSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleSocket object is destroyed.
  OutputStream& GetOutputStream() { return impl_->GetOutputStream(); }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() { return impl_->Close(); }

  // Returns true if a socket is usable. If this method returns false,
  // it is not safe to call any other method.
  // NOTE(socket validity):
  // Socket created by a default public constructor is not valid, because
  // it is missing platform implementation.
  // The only way to obtain a valid socket is through connection, such as
  // an object returned by WifiLanMedium::Connect
  // These methods may also return an invalid socket if connection failed for
  // any reason.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  // Returned reference will remain valid for while BleSocket object is
  // itself valid. Typically BleSocket lifetime matches duration of the
  // connection, and is controlled by end user, since they hold the instance.
  api::ble_v2::BleSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::ble_v2::BleSocket> impl_;
};

class BleServerSocket final {
 public:
  explicit BleServerSocket(std::unique_ptr<api::ble_v2::BleServerSocket> socket)
      : impl_(std::move(socket)) {}
  BleServerSocket(const BleServerSocket&) = default;
  BleServerSocket& operator=(const BleServerSocket&) = default;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  BleSocket Accept() {
    std::unique_ptr<api::ble_v2::BleSocket> socket = impl_->Accept();
    if (!socket) {
      NEARBY_LOGS(INFO)
          << "BleServerSocket Accept() failed on server socket: " << this;
    }
    return BleSocket(std::move(socket));
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    NEARBY_LOGS(INFO) << "BleServerSocket Closing:: " << this;
    return impl_->Close();
  }

  bool IsValid() const { return impl_ != nullptr; }
  api::ble_v2::BleServerSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::ble_v2::BleServerSocket> impl_;
};

// Container of operations that can be performed over the BLE medium.
class BleV2Medium final {
 public:
  struct ScanCallback {
    std::function<void(
        BleV2Peripheral& peripheral,
        const api::ble_v2::BleAdvertisementData& advertisement_data)>
        advertisement_found_cb = location::nearby::DefaultCallback<
            BleV2Peripheral&, const api::ble_v2::BleAdvertisementData&>();
  };

  struct ServerGattConnectionCallback {
    std::function<void(ServerGattConnection& connection,
                       const api::ble_v2::GattCharacteristic& characteristic)>
        characteristic_subscription_cb = location::nearby::DefaultCallback<
            ServerGattConnection&, const api::ble_v2::GattCharacteristic&>();
    std::function<void(ServerGattConnection& connection,
                       const api::ble_v2::GattCharacteristic& characteristic)>
        characteristic_unsubscription_cb = location::nearby::DefaultCallback<
            ServerGattConnection&, const api::ble_v2::GattCharacteristic&>();
  };

  struct ClientGattConnectionCallback {
   public:
    std::function<void(ClientGattConnection& connection)> disconnected_cb =
        location::nearby::DefaultCallback<ClientGattConnection&>();
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
  bool StartScanning(const std::vector<std::string>& service_uuids,
                     api::ble_v2::PowerMode power_mode, ScanCallback callback);
  bool StopScanning();

  std::unique_ptr<GattServer> StartGattServer(
      ServerGattConnectionCallback callback);

  // Returns a new BleSocket. On Success, BleSocket::IsValid()
  // returns true.
  ClientGattConnection ConnectToGattServer(
      BleV2Peripheral& peripheral, int mtu, api::ble_v2::PowerMode power_mode,
      ClientGattConnectionCallback callback);
  bool IsValid() const { return impl_ != nullptr; }

  BleServerSocket OpenServerSocket(
      const std::string& service_id);

  BleSocket Connect(const std::string& service_id,
                    api::ble_v2::PowerMode power_mode,
                    BleV2Peripheral& peripheral,
                    CancellationFlag* cancellation_flag);

  api::ble_v2::BleMedium* GetImpl() const { return impl_.get(); }

 private:
  Mutex mutex_;
  std::unique_ptr<api::ble_v2::BleMedium> impl_;
  BluetoothAdapter& adapter_;
  ServerGattConnectionCallback server_gatt_connection_callback_
      ABSL_GUARDED_BY(mutex_);
  ClientGattConnectionCallback client_gatt_connection_callback_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<api::ble_v2::BlePeripheral*,
                      std::unique_ptr<BleV2Peripheral>>
      peripherals_ ABSL_GUARDED_BY(mutex_);
  ScanCallback scan_callback_ ABSL_GUARDED_BY(mutex_);
  bool scanning_enabled_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_BLE_V2_H_
