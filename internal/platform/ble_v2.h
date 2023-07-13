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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/uuid.h"

namespace nearby {

class BleV2Medium;

// Opaque wrapper over a BLE peripheral. Must contain enough data about a
// particular BLE peripheral to connect to its GATT server.
class BleV2Peripheral final {
 public:
  using ImplCallback =
      absl::AnyInvocable<void(api::ble_v2::BlePeripheral& device)>;
  BleV2Peripheral() = default;
  BleV2Peripheral(BleV2Medium& medium, api::ble_v2::BlePeripheral& impl)
      : medium_(&medium), unique_id_(impl.GetUniqueId()) {}
  BleV2Peripheral(const BleV2Peripheral&) = default;
  BleV2Peripheral& operator=(const BleV2Peripheral&) = default;
  BleV2Peripheral(BleV2Peripheral&& other) = default;

  BleV2Peripheral& operator=(BleV2Peripheral&& other) = default;

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<std::string> GetAddress() const;
  ByteArray GetId() const { return id_; }
  void SetId(const ByteArray& id) { id_ = id; }

  int GetPsm() const { return psm_; }
  void SetPsm(int psm) { psm_ = psm; }

  bool IsValid() const;
  explicit operator bool() const { return IsValid(); }

  bool GetImpl(ImplCallback callback) const;

 private:
  BleV2Medium* medium_ = nullptr;
  std::optional<api::ble_v2::BlePeripheral::UniqueId> unique_id_;

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
class BleV2Socket final {
 public:
  BleV2Socket() = default;
  BleV2Socket(BleV2Peripheral peripheral,
              std::unique_ptr<api::ble_v2::BleSocket> socket)
      : peripheral_(peripheral) {
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
  BleV2Peripheral& GetRemotePeripheral() { return peripheral_; }

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
  BleV2Peripheral peripheral_;
};

// Container of operations that can be performed over the BLE GATT server
// socket.
// This class is copyable but not moveable.
class BleV2ServerSocket final {
 public:
  BleV2ServerSocket(BleV2Medium& medium,
                    std::unique_ptr<api::ble_v2::BleServerSocket> socket)
      : medium_(&medium), impl_(std::move(socket)) {}
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
    BleV2Peripheral peripheral;
    if (!socket) {
      NEARBY_LOGS(INFO) << "BleServerSocket Accept() failed on server socket: "
                        << this;
    } else {
      auto* platform_peripheral = socket->GetRemotePeripheral();
      if (platform_peripheral != nullptr) {
        peripheral = BleV2Peripheral(*medium_, *platform_peripheral);
      }
    }
    return BleV2Socket(peripheral, std::move(socket));
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    NEARBY_LOGS(INFO) << "BleServerSocket Closing:: " << this;
    return impl_->Close();
  }

  bool IsValid() const { return impl_ != nullptr; }
  api::ble_v2::BleServerSocket& GetImpl() { return *impl_; }

 private:
  BleV2Medium* medium_;
  std::shared_ptr<api::ble_v2::BleServerSocket> impl_;
};

// Opaque wrapper over a GattServer.
// Move only, disallow copy.
//
// Note that some of the methods return absl::optional instead
// of std::optional, because iOS platform is still in C++14.
// LINT.IfChange
class GattServer final {
 public:
  GattServer(BleV2Medium& medium,
             std::unique_ptr<api::ble_v2::GattServer> gatt_server)
      : medium_(medium), impl_(std::move(gatt_server)) {}
  ~GattServer() { Stop(); }

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid,
      const api::ble_v2::GattCharacteristic::Permission permission,
      const api::ble_v2::GattCharacteristic::Property property) {
    return impl_->CreateCharacteristic(service_uuid, characteristic_uuid,
                                       permission, property);
  }

  bool UpdateCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      const ByteArray& value) {
    return impl_->UpdateCharacteristic(characteristic, value);
  }

  absl::Status NotifyCharacteristicChanged(
      const api::ble_v2::GattCharacteristic& characteristic, bool confirm,
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

  BleV2Peripheral GetBlePeripheral() {
    return BleV2Peripheral(medium_, impl_->GetBlePeripheral());
  }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging
  // purposes.
  api::ble_v2::GattServer* GetImpl() { return impl_.get(); }

 private:
  BleV2Medium& medium_;
  std::unique_ptr<api::ble_v2::GattServer> impl_;
};
// LINT.ThenChange(//depot/google3/third_party/nearby/internal/platform/implementation/ble_v2.h)

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
  absl::optional<std::string> ReadCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic) {
    return impl_->ReadCharacteristic(characteristic);
  }

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  bool WriteCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      absl::string_view value, api::ble_v2::GattClient::WriteType write_type) {
    return impl_->WriteCharacteristic(characteristic, value, write_type);
  }

  // TODO(qinwangz): We should not need  `on_characteristic_changed_cb` when
  // unsubscribing.
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  bool SetCharacteristicSubscription(
      const api::ble_v2::GattCharacteristic& characteristic, bool enable,
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
    using BlePeripheral = api::ble_v2::BlePeripheral;
    using GattCharacteristic = api::ble_v2::GattCharacteristic;
    using ReadValueCallback =
        api::ble_v2::ServerGattConnectionCallback::ReadValueCallback;
    using WriteValueCallback =
        api::ble_v2::ServerGattConnectionCallback::WriteValueCallback;

    absl::AnyInvocable<void(const GattCharacteristic& characteristic)>
        characteristic_subscription_cb =
            nearby::DefaultCallback<const GattCharacteristic&>();
    absl::AnyInvocable<void(const GattCharacteristic& characteristic)>
        characteristic_unsubscription_cb =
            nearby::DefaultCallback<const GattCharacteristic&>();
    absl::AnyInvocable<void(const BlePeripheral& remote_device,
                            const GattCharacteristic& characteristic,
                            int offset, ReadValueCallback callback)>
        on_characteristic_read_cb;
    absl::AnyInvocable<void(const BlePeripheral& remote_device,
                            const GattCharacteristic& characteristic,
                            int offset, absl::string_view data,
                            WriteValueCallback callback)>
        on_characteristic_write_cb;
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

  ~BleV2Medium();
  // Returns true once the BLE advertising has been initiated.
  // This interface will be deprecated soon.
  // TODO(b/271305977) remove this function.
  // Use 'unique_ptr<AdvertisingSession> StartAdvertisingTmp' instead.
  bool StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_parameters);
  // This interface will be deprecated soon.
  // TODO(b/271305977) remove this function.
  bool StopAdvertising();

  // Temp interface for windows client to use before windows has native impl
  // for 'unique_ptr<AdvertisingSession> StartAdvertising'.
  // TODO(b/271305977) remove this function.
  std::unique_ptr<api::ble_v2::BleMedium::AdvertisingSession>
  StartAdvertisingTmp(const api::ble_v2::BleAdvertisementData& advertising_data,
                      api::ble_v2::AdvertiseParameters advertise_set_parameters,
                      api::ble_v2::BleMedium::AdvertisingCallback callback);

  std::unique_ptr<api::ble_v2::BleMedium::AdvertisingSession> StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_set_parameters,
      api::ble_v2::BleMedium::AdvertisingCallback callback);

  // Returns true once the BLE scan has been initiated.
  // This interface will be deprecated soon.
  // TODO(b/271305977) remove this function.
  // Use 'unique_ptr<ScanningSession> StartScanningTmp' instead.
  bool StartScanning(const Uuid& service_uuid,
                     api::ble_v2::TxPowerLevel tx_power_level,
                     ScanCallback callback);
  // This interface will be deprecated soon.
  // TODO(b/271305977) remove this function.
  bool StopScanning();

  std::unique_ptr<api::ble_v2::BleMedium::ScanningSession> StartScanning(
      const Uuid& service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::BleMedium::ScanningCallback callback);

  // Temp interface for windows client to use before windows has native impl
  // for 'unique_ptr<AdvertisingSession> StartScanning'.
  // TODO(b/271305977) remove this function.
  std::unique_ptr<api::ble_v2::BleMedium::ScanningSession> StartScanningTmp(
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

  // Returns a `BleV2Peripheral` with given mac address. `mac_address` is in
  // canonical format.
  BleV2Peripheral GetRemotePeripheral(const std::string& mac_address);

  api::ble_v2::BleMedium* GetImpl() const { return impl_.get(); }
  BluetoothAdapter& GetAdapter() { return adapter_; }

 private:
  Mutex mutex_;
  std::unique_ptr<api::ble_v2::BleMedium> impl_;
  BluetoothAdapter& adapter_;
  ServerGattConnectionCallback server_gatt_connection_callback_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<api::ble_v2::BlePeripheral*> peripherals_
      ABSL_GUARDED_BY(mutex_);
  ScanCallback scan_callback_ ABSL_GUARDED_BY(mutex_);
  bool scanning_enabled_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_BLE_V2_H_
