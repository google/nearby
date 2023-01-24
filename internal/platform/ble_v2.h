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
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/uuid.h"

namespace nearby {

// Container of operations that can be performed over the BLE GATT client
// socket.
// This class is copyable but not moveable.
class BleV2Socket final {
 public:
  BleV2Socket() = default;
  explicit BleV2Socket(std::unique_ptr<api::ble_v2::BleSocket> socket) {
    state_->socket = std::move(socket);
  }
  BleV2Socket(const BleV2Socket&) = default;
  BleV2Socket& operator=(const BleV2Socket&) = default;

  // Returns the InputStream of the BleSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleSocket object is destroyed.
  InputStream& GetInputStream() { return state_->socket->GetInputStream(); }

  // Returns the OutputStream of the BleSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleSocket object is destroyed.
  OutputStream& GetOutputStream() { return state_->socket->GetOutputStream(); }

  // Sets the close notifier by client side.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier) {
    state_->close_notifier = std::move(notifier);
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    if (state_->close_notifier) {
      auto notifier = std::move(state_->close_notifier);
      notifier();
    }
    return state_->socket->Close();
  }

  // Returns BlePeripheral object which wraps a valid BlePeripheral pointer.
  BleV2Peripheral GetRemotePeripheral() {
    return BleV2Peripheral(state_->socket->GetRemotePeripheral());
  }

  // Returns true if a socket is usable. If this method returns false,
  // it is not safe to call any other method.
  // NOTE(socket validity):
  // Socket created by a default public constructor is not valid, because
  // it is missing platform implementation.
  // The only way to obtain a valid socket is through connection, such as
  // an object returned by BleMedium::Connect
  // These methods may also return an invalid socket if connection failed for
  // any reason.
  bool IsValid() const { return state_->socket != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  // Returned reference will remain valid for while BleSocket object is
  // itself valid. Typically BleSocket lifetime matches duration of the
  // connection, and is controlled by end user, since they hold the instance.
  api::ble_v2::BleSocket& GetImpl() { return *state_->socket; }

 private:
  struct SharedState {
    std::unique_ptr<api::ble_v2::BleSocket> socket;
    absl::AnyInvocable<void()> close_notifier;
  };
  std::shared_ptr<SharedState> state_ = std::make_shared<SharedState>();
};

// Container of operations that can be performed over the BLE GATT server
// socket.
// This class is copyable but not moveable.
class BleV2ServerSocket final {
 public:
  explicit BleV2ServerSocket(
      std::unique_ptr<api::ble_v2::BleServerSocket> socket)
      : impl_(std::move(socket)) {}
  BleV2ServerSocket(const BleV2ServerSocket&) = default;
  BleV2ServerSocket& operator=(const BleV2ServerSocket&) = default;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // On error, "impl_" will be nullptr and the caller will check it by calling
  // member function "IsValid()"
  // Once error is reported, it is permanent, and
  // ServerSocket has to be closed by caller.
  BleV2Socket Accept() {
    std::unique_ptr<api::ble_v2::BleSocket> socket = impl_->Accept();
    if (!socket) {
      NEARBY_LOGS(INFO) << "BleServerSocket Accept() failed on server socket: "
                        << this;
    }
    return BleV2Socket(std::move(socket));
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
      const Uuid& service_uuid, const Uuid& characteristic_uuid,
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

  void Stop() {
    if (impl_) return impl_->Stop();
  }

  // Returns true if a gatt_server is usable. If this method returns false,
  // it is not safe to call any other method.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging
  // purposes.
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

  bool DiscoverServiceAndCharacteristics(
      const Uuid& service_uuid, const std::vector<Uuid>& characteristic_uuids) {
    return impl_->DiscoverServiceAndCharacteristics(service_uuid,
                                                    characteristic_uuids);
  }

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid) {
    return impl_->GetCharacteristic(service_uuid, characteristic_uuid);
  }

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<ByteArray> ReadCharacteristic(
      api::ble_v2::GattCharacteristic& characteristic) {
    return impl_->ReadCharacteristic(characteristic);
  }

  void Disconnect() { impl_->Disconnect(); }

  // Returns true if a client_gatt_connection is usable. If this method
  // returns false, it is not safe to call any other method.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging
  // purposes.
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
    absl::AnyInvocable<void(
        BleV2Peripheral peripheral,
        const api::ble_v2::BleAdvertisementData& advertisement_data)>
        advertisement_found_cb =
            nearby::DefaultCallback<BleV2Peripheral,
                                    const api::ble_v2::BleAdvertisementData&>();
  };
  struct ServerGattConnectionCallback {
    absl::AnyInvocable<void(
        const api::ble_v2::GattCharacteristic& characteristic)>
        characteristic_subscription_cb =
            nearby::DefaultCallback<const api::ble_v2::GattCharacteristic&>();
    absl::AnyInvocable<void(
        const api::ble_v2::GattCharacteristic& characteristic)>
        characteristic_unsubscription_cb =
            nearby::DefaultCallback<const api::ble_v2::GattCharacteristic&>();
  };
  // TODO(b/231318879): Remove this wrapper callback and use impl callback if
  // there is only disconnect function here in the end.
  struct ClientGattConnectionCallback {
    absl::AnyInvocable<void()> disconnected_cb = nearby::DefaultCallback<>();
  };

  explicit BleV2Medium(BluetoothAdapter& adapter)
      : impl_(
            api::ImplementationPlatform::CreateBleV2Medium(adapter.GetImpl())),
        adapter_(adapter) {}

  // Returns true once the BLE advertising has been initiated.
  bool StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_parameters);
  bool StopAdvertising();

  std::unique_ptr<api::ble_v2::BleMedium::AdvertisingSession> StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters,
      api::ble_v2::BleMedium::AdvertisingCallback callback);

  // Returns true once the BLE scan has been initiated.
  bool StartScanning(const Uuid& service_uuid,
                     api::ble_v2::TxPowerLevel tx_power_level,
                     ScanCallback callback);
  bool StopScanning();

  std::unique_ptr<api::ble_v2::BleMedium::ScanningSession> StartScanning(
      const Uuid& service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::BleMedium::ScanningCallback callback);

  // Starts Gatt Server for waiting to client connection.
  std::unique_ptr<GattServer> StartGattServer(
      ServerGattConnectionCallback callback);

  // Returns a new GattClient connection to a gatt server.
  // There is only one instance of GattServer can run at a time.
  std::unique_ptr<GattClient> ConnectToGattServer(
      BleV2Peripheral peripheral, api::ble_v2::TxPowerLevel tx_power_level,
      ClientGattConnectionCallback callback);

  // Returns a new BleServerSocket.
  // On Success, BleServerSocket::IsValid() returns true.
  BleV2ServerSocket OpenServerSocket(const std::string& service_id);

  // Returns a new BleLanSocket.
  // On Success, BleLanSocket::IsValid() returns true.
  BleV2Socket Connect(const std::string& service_id,
                      api::ble_v2::TxPowerLevel tx_power_level,
                      const BleV2Peripheral& peripheral,
                      CancellationFlag* cancellation_flag);

  bool IsExtendedAdvertisementsAvailable();

  bool IsValid() const { return impl_ != nullptr; }

  api::ble_v2::BleMedium* GetImpl() const { return impl_.get(); }
  BluetoothAdapter& GetAdapter() { return adapter_; }

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

#endif  // PLATFORM_PUBLIC_BLE_V2_H_
