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

#ifndef PLATFORM_PUBLIC_BLE_H_
#define PLATFORM_PUBLIC_BLE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/uuid.h"

namespace nearby {

class BleMedium;

// TODO: b/399815436 - Remove all the shared_ptr in this file.
// Opaque wrapper over a BLE peripheral. Must contain enough data about a
// particular BLE peripheral to connect to its GATT server.
class BlePeripheral final {
 public:
  BlePeripheral() = default;
  BlePeripheral(BleMedium& medium, api::ble::BlePeripheral::UniqueId unique_id)
      : medium_(&medium), unique_id_(unique_id) {}
  BlePeripheral(const BlePeripheral&) = default;
  BlePeripheral& operator=(const BlePeripheral&) = default;
  BlePeripheral(BlePeripheral&& other) = default;

  BlePeripheral& operator=(BlePeripheral&& other) = default;

  ByteArray GetId() const { return id_; }
  void SetId(const ByteArray& id) { id_ = id; }

  int GetPsm() const { return psm_; }
  void SetPsm(int psm) { psm_ = psm; }

  bool IsValid() const;

  std::optional<api::ble::BlePeripheral::UniqueId> GetUniqueId() const {
    return unique_id_;
  }

  api::ble::BlePeripheral* GetImpl() const;
  std::string ToReadableString() const {
    if (!IsValid()) {
      return "BlePeripheral { invalid }";
    }
    return absl::StrFormat("BlePeripheral { id=%s, psm=%d}",
                           absl::BytesToHexString(GetId().AsStringView()),
                           GetPsm());
  }

 private:
  BleMedium* medium_ = nullptr;
  std::optional<api::ble::BlePeripheral::UniqueId> unique_id_;

  // A unique identifier for this peripheral. It is the BLE advertisement bytes
  // it was found on.
  ByteArray id_ = {};

  // The psm (protocol service multiplexer) value is used for create data
  // connection on L2CAP socket. It only exists when remote device supports
  // L2CAP socket feature.
  int psm_ = 0;
};

// Container of operations that can be performed over the BLE GATT client
// socket.
// This class is copyable but not moveable.
class BleSocket final {
 public:
  BleSocket() = default;
  BleSocket(BlePeripheral peripheral,
            std::unique_ptr<api::ble::BleSocket> socket)
      : peripheral_(peripheral) {
    state_->socket = std::move(socket);
  }
  BleSocket(const BleSocket&) = default;
  BleSocket& operator=(const BleSocket&) = default;

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
    if (state_->close_notifier != nullptr) {
      absl::AnyInvocable<void()> notifier = std::move(state_->close_notifier);
      notifier();
    }
    return state_->socket->Close();
  }

  // Returns BlePeripheral object which wraps a valid BlePeripheral pointer.
  BlePeripheral& GetRemotePeripheral() { return peripheral_; }

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
  api::ble::BleSocket& GetImpl() { return *state_->socket; }

 private:
  struct SharedState {
    std::unique_ptr<api::ble::BleSocket> socket;
    absl::AnyInvocable<void()> close_notifier;
  };
  std::shared_ptr<SharedState> state_ = std::make_shared<SharedState>();
  BlePeripheral peripheral_;
};

// Container of operations that can be performed over the BLE GATT server
// socket.
// This class is copyable but not moveable.
class BleServerSocket final {
 public:
  BleServerSocket(BleMedium& medium,
                  std::unique_ptr<api::ble::BleServerSocket> socket)
      : medium_(&medium), impl_(std::move(socket)) {}
  BleServerSocket(const BleServerSocket&) = default;
  BleServerSocket& operator=(const BleServerSocket&) = default;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // On error, "impl_" will be nullptr and the caller will check it by calling
  // member function "IsValid()"
  // Once error is reported, it is permanent, and
  // ServerSocket has to be closed by caller.
  BleSocket Accept() {
    std::unique_ptr<api::ble::BleSocket> socket = impl_->Accept();
    BlePeripheral peripheral;
    if (socket == nullptr) {
      LOG(INFO) << "BleServerSocket Accept() failed on server socket: " << this;
    } else {
      peripheral = BlePeripheral(*medium_, socket->GetRemotePeripheralId());
    }
    return BleSocket(peripheral, std::move(socket));
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    LOG(INFO) << "BleServerSocket Closing:: " << this;
    return impl_->Close();
  }

  bool IsValid() const { return impl_ != nullptr; }
  api::ble::BleServerSocket& GetImpl() { return *impl_; }

 private:
  BleMedium* medium_;
  std::shared_ptr<api::ble::BleServerSocket> impl_;
};

// Opaque wrapper over a GattServer.
// Move only, disallow copy.
//
// Note that some of the methods return absl::optional instead
// of std::optional, because iOS platform is still in C++14.
class GattServer final {
 public:
  explicit GattServer(std::unique_ptr<api::ble::GattServer> gatt_server)
      : impl_(std::move(gatt_server)) {}
  ~GattServer() { Stop(); }

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<api::ble::GattCharacteristic> CreateCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid,
      const api::ble::GattCharacteristic::Permission permission,
      const api::ble::GattCharacteristic::Property property) {
    return impl_->CreateCharacteristic(service_uuid, characteristic_uuid,
                                       permission, property);
  }

  bool UpdateCharacteristic(const api::ble::GattCharacteristic& characteristic,
                            const ByteArray& value) {
    return impl_->UpdateCharacteristic(characteristic, value);
  }

  absl::Status NotifyCharacteristicChanged(
      const api::ble::GattCharacteristic& characteristic, bool confirm,
      const ByteArray& new_value) {
    return impl_->NotifyCharacteristicChanged(characteristic, confirm,
                                              new_value);
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
  api::ble::GattServer* GetImpl() { return impl_.get(); }

 private:
  std::unique_ptr<api::ble::GattServer> impl_;
};

// Opaque wrapper for a GattClient.
//
// Note that some of the methods return absl::optional instead
// of std::optional, because iOS platform is still in C++14.
class GattClient final {
 public:
  explicit GattClient(
      std::unique_ptr<api::ble::GattClient> client_gatt_connection)
      : impl_(std::move(client_gatt_connection)) {}

  bool DiscoverServiceAndCharacteristics(
      const Uuid& service_uuid, const std::vector<Uuid>& characteristic_uuids) {
    return impl_->DiscoverServiceAndCharacteristics(service_uuid,
                                                    characteristic_uuids);
  }

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<api::ble::GattCharacteristic> GetCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid) {
    return impl_->GetCharacteristic(service_uuid, characteristic_uuid);
  }

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<std::string> ReadCharacteristic(
      const api::ble::GattCharacteristic& characteristic) {
    return impl_->ReadCharacteristic(characteristic);
  }

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  bool WriteCharacteristic(const api::ble::GattCharacteristic& characteristic,
                           absl::string_view value,
                           api::ble::GattClient::WriteType write_type) {
    return impl_->WriteCharacteristic(characteristic, value, write_type);
  }

  // TODO(qinwangz): We should not need  `on_characteristic_changed_cb` when
  // unsubscribing.
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  bool SetCharacteristicSubscription(
      const api::ble::GattCharacteristic& characteristic, bool enable,
      absl::AnyInvocable<void(absl::string_view value)>
          on_characteristic_changed_cb) {
    return impl_->SetCharacteristicSubscription(
        characteristic, enable, std::move(on_characteristic_changed_cb));
  }

  void Disconnect() { impl_->Disconnect(); }

  // Returns true if a client_gatt_connection is usable. If this method
  // returns false, it is not safe to call any other method.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging
  // purposes.
  api::ble::GattClient* GetImpl() { return impl_.get(); }

 private:
  std::unique_ptr<api::ble::GattClient> impl_;
};

class BleL2capSocket final {
 public:
  BleL2capSocket() = default;
  BleL2capSocket(BlePeripheral peripheral,
                 std::unique_ptr<api::ble::BleL2capSocket> socket)
      : peripheral_(peripheral) {
    state_->socket = std::move(socket);
  }
  BleL2capSocket(const BleL2capSocket&) = default;
  BleL2capSocket& operator=(const BleL2capSocket&) = default;

  // Returns the InputStream of the BleL2capSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleL2capSocket object is destroyed.
  InputStream& GetInputStream() { return state_->socket->GetInputStream(); }

  // Returns the OutputStream of the BleL2capSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the BleL2capSocket object is destroyed.
  OutputStream& GetOutputStream() { return state_->socket->GetOutputStream(); }

  // Sets the close notifier by client side.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier) {
    state_->close_notifier = std::move(notifier);
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    if (state_->close_notifier != nullptr) {
      absl::AnyInvocable<void()> notifier = std::move(state_->close_notifier);
      notifier();
    }
    return state_->socket->Close();
  }

  // Returns BlePeripheral object which wraps a valid BlePeripheral pointer.
  BlePeripheral& GetRemotePeripheral() { return peripheral_; }

  // Returns true if a socket is usable. If this method returns false,
  // it is not safe to call any other method.
  // NOTE(socket validity):
  // Socket created by a default public constructor is not valid, because
  // it is missing platform implementation.
  // The only way to obtain a valid socket is through connection, such as
  // an object returned by BleMedium::ConnectOverL2cap.
  // These methods may also return an invalid socket if connection failed for
  // any reason.
  bool IsValid() const { return state_->socket != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  // Returned reference will remain valid for while BleSocket object is
  // itself valid. Typically BleSocket lifetime matches duration of the
  // connection, and is controlled by end user, since they hold the instance.
  api::ble::BleL2capSocket& GetImpl() { return *state_->socket; }

 private:
  struct SharedState {
    std::unique_ptr<api::ble::BleL2capSocket> socket;
    absl::AnyInvocable<void()> close_notifier;
  };
  std::shared_ptr<SharedState> state_ = std::make_shared<SharedState>();
  BlePeripheral peripheral_;
};

class BleL2capServerSocket final {
 public:
  BleL2capServerSocket(BleMedium& medium,
                       std::unique_ptr<api::ble::BleL2capServerSocket> socket)
      : medium_(&medium), impl_(std::move(socket)) {}
  BleL2capServerSocket(const BleL2capServerSocket&) = default;
  BleL2capServerSocket& operator=(const BleL2capServerSocket&) = default;
  // Gets PSM value has been published by the server.
  int GetPSM() { return impl_->GetPSM(); }

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // On error, "impl_" will be nullptr and the caller will check it by calling
  // member function "IsValid()"
  // Once error is reported, it is permanent, and
  // ServerSocket has to be closed by caller.
  BleL2capSocket Accept() {
    std::unique_ptr<api::ble::BleL2capSocket> socket = impl_->Accept();
    BlePeripheral peripheral;
    if (socket == nullptr) {
      LOG(INFO) << "BleL2capServerSocket Accept() failed on server socket: "
                << this;
    } else {
      peripheral = BlePeripheral(*medium_, socket->GetRemotePeripheralId());
    }
    return BleL2capSocket(peripheral, std::move(socket));
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    LOG(INFO) << "BleL2capServerSocket Closing:: " << this;
    return impl_->Close();
  }

  // Returns true if a BleL2capServerSocket is usable. If this method returns
  // false, it is not safe to call any other method.
  bool IsValid() const { return impl_ != nullptr; }
  api::ble::BleL2capServerSocket& GetImpl() { return *impl_; }

 private:
  BleMedium* medium_ = nullptr;
  std::shared_ptr<api::ble::BleL2capServerSocket> impl_;
};

// Container of operations that can be performed over the BLE medium.
class BleMedium final {
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
        BlePeripheral peripheral,
        const api::ble::BleAdvertisementData& advertisement_data)>
        advertisement_found_cb =
            nearby::DefaultCallback<BlePeripheral,
                                    const api::ble::BleAdvertisementData&>();
  };

  struct ServerGattConnectionCallback {
    absl::AnyInvocable<void(const api::ble::GattCharacteristic& characteristic)>
        characteristic_subscription_cb =
            nearby::DefaultCallback<const api::ble::GattCharacteristic&>();
    absl::AnyInvocable<void(const api::ble::GattCharacteristic& characteristic)>
        characteristic_unsubscription_cb =
            nearby::DefaultCallback<const api::ble::GattCharacteristic&>();
    absl::AnyInvocable<void(
        const api::ble::BlePeripheral::UniqueId remote_device_id,
        const api::ble::GattCharacteristic& characteristic, int offset,
        api::ble::ServerGattConnectionCallback::ReadValueCallback callback)>
        on_characteristic_read_cb;
    absl::AnyInvocable<void(
        const api::ble::BlePeripheral::UniqueId remote_device_id,
        const api::ble::GattCharacteristic& characteristic, int offset,
        absl::string_view data,
        api::ble::ServerGattConnectionCallback::WriteValueCallback callback)>
        on_characteristic_write_cb;
  };
  // TODO(b/231318879): Remove this wrapper callback and use impl callback if
  // there is only disconnect function here in the end.
  struct ClientGattConnectionCallback {
    absl::AnyInvocable<void()> disconnected_cb = nearby::DefaultCallback<>();
  };

  explicit BleMedium(BluetoothAdapter& adapter)
      : impl_(api::ImplementationPlatform::CreateBleMedium(adapter.GetImpl())),
        adapter_(adapter) {}

  ~BleMedium();
  // Returns true once the BLE advertising has been initiated.
  // This interface will be deprecated soon.
  // TODO(b/271305977) remove this function.
  // Use 'unique_ptr<AdvertisingSession> StartAdvertising' instead.
  bool StartAdvertising(const api::ble::BleAdvertisementData& advertising_data,
                        api::ble::AdvertiseParameters advertise_parameters);
  // This interface will be deprecated soon.
  // TODO(b/271305977) remove this function.
  bool StopAdvertising();

  std::unique_ptr<api::ble::BleMedium::AdvertisingSession> StartAdvertising(
      const api::ble::BleAdvertisementData& advertising_data,
      api::ble::AdvertiseParameters advertise_set_parameters,
      api::ble::BleMedium::AdvertisingCallback callback);

  // Returns true once the BLE scan has been initiated.
  // This interface will be deprecated soon.
  bool StartScanning(const Uuid& service_uuid,
                     api::ble::TxPowerLevel tx_power_level,
                     ScanCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once the BLE multiple services scan has been initiated.
  bool StartMultipleServicesScanning(const std::vector<Uuid>& service_uuids,
                                     api::ble::TxPowerLevel tx_power_level,
                                     ScanCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // This interface will be deprecated soon.
  // TODO(b/271305977) remove this function.
  bool StopScanning();

  // Pause BLE scanning at platform Medium level.
  bool PauseMediumScanning();

  // Resume BLE scanning at platform Medium level.
  bool ResumeMediumScanning();

  std::unique_ptr<api::ble::BleMedium::ScanningSession> StartScanning(
      const Uuid& service_uuid, api::ble::TxPowerLevel tx_power_level,
      api::ble::BleMedium::ScanningCallback callback);

  // Starts Gatt Server for waiting to client connection.
  std::unique_ptr<GattServer> StartGattServer(
      ServerGattConnectionCallback callback);

  // Returns a new GattClient connection to a gatt server.
  // There is only one instance of GattServer can run at a time.
  std::unique_ptr<GattClient> ConnectToGattServer(
      BlePeripheral peripheral, api::ble::TxPowerLevel tx_power_level,
      ClientGattConnectionCallback callback);

  // Returns a new BleServerSocket.
  // On Success, BleServerSocket::IsValid() returns true.
  BleServerSocket OpenServerSocket(const std::string& service_id);

  // Returns a new BleL2capServerSocket.
  // On Success, BleL2capServerSocket::IsValid() returns true.
  BleL2capServerSocket OpenL2capServerSocket(const std::string& service_id);

  // Returns a new BleSocket.
  // On Success, BleSocket::IsValid() returns true.
  BleSocket Connect(const std::string& service_id,
                    api::ble::TxPowerLevel tx_power_level,
                    const BlePeripheral& peripheral,
                    CancellationFlag* cancellation_flag);

  // Returns a new BleL2capSocket.
  // On Success, BleL2capSocket::IsValid() returns true.
  BleL2capSocket ConnectOverL2cap(const std::string& service_id,
                                  api::ble::TxPowerLevel tx_power_level,
                                  const BlePeripheral& peripheral,
                                  CancellationFlag* cancellation_flag);

  bool IsExtendedAdvertisementsAvailable();

  bool IsValid() const { return impl_ != nullptr; }

  api::ble::BleMedium* GetImpl() const { return impl_.get(); }
  BluetoothAdapter& GetAdapter() { return adapter_; }
  void AddAlternateUuidForService(uint16_t uuid,
                                  const std::string& service_id) {
    impl_->AddAlternateUuidForService(uuid, service_id);
  }

 private:
  Mutex mutex_;
  std::unique_ptr<api::ble::BleMedium> impl_;
  BluetoothAdapter& adapter_;
  ServerGattConnectionCallback server_gatt_connection_callback_
      ABSL_GUARDED_BY(mutex_);
  ScanCallback scan_callback_ ABSL_GUARDED_BY(mutex_);
  bool scanning_enabled_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_BLE_H_
