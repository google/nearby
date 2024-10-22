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

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/borrowable.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/g3/bluetooth_adapter.h"
#include "internal/platform/implementation/g3/socket_base.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace g3 {

// BlePeripheral implementation.
class BleV2Peripheral : public api::ble_v2::BlePeripheral {
 public:
  explicit BleV2Peripheral(BluetoothAdapter* adapter);
  std::string GetAddress() const override;
  api::ble_v2::BlePeripheral::UniqueId GetUniqueId() const override;

  BluetoothAdapter& GetAdapter() { return adapter_; }

 private:
  BluetoothAdapter& adapter_;
};

class BleV2Socket : public api::ble_v2::BleSocket, public SocketBase {
 public:
  explicit BleV2Socket(BluetoothAdapter* adapter) : adapter_(adapter) {}

  // Returns the InputStream of this connected BleSocket.
  InputStream& GetInputStream() override {
    return SocketBase::GetInputStream();
  }

  // Returns the OutputStream of this connected BleSocket.
  // This stream is for local side to write.
  OutputStream& GetOutputStream() override {
    return SocketBase::GetOutputStream();
  }

  // Returns address of a remote BleSocket or nullptr.
  BleV2Socket* GetRemoteSocket() {
    return static_cast<BleV2Socket*>(SocketBase::GetRemoteSocket());
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override { return SocketBase::Close(); }

  // Returns valid BlePeripheral pointer if there is a connection, and
  // nullptr otherwise.
  BleV2Peripheral* GetRemotePeripheral() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  BluetoothAdapter* adapter_ = nullptr;  // Our Adapter. Read only.
};

class BleV2ServerSocket : public api::ble_v2::BleServerSocket {
 public:
  explicit BleV2ServerSocket(BluetoothAdapter* adapter) : adapter_(adapter) {}
  BleV2ServerSocket(const BleV2ServerSocket&) = default;
  BleV2ServerSocket& operator=(const BleV2ServerSocket&) = default;
  BleV2ServerSocket(BleV2ServerSocket&&) = default;
  BleV2ServerSocket& operator=(BleV2ServerSocket&&) = default;
  ~BleV2ServerSocket() override;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  //
  // Called by the server side of a connection.
  // Returns BleSocket to the server side.
  // If not null, returned socket is connected to its remote (client-side) peer.
  std::unique_ptr<api::ble_v2::BleSocket> Accept() override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Blocks until either:
  // - connection is available, or
  // - server socket is closed, or
  // - error happens.
  //
  // Called by the client side of a connection.
  // Returns true, if socket is successfully connected.
  bool Connect(BleV2Socket& socket) ABSL_LOCKS_EXCLUDED(mutex_);

  // Called by the server side of a connection before passing ownership of
  // BleServerSocker to user, to track validity of a pointer to this
  // server socket.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // Calls close_notifier if it was previously set, and marks socket as closed.
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Exception DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  absl::CondVar cond_;
  BluetoothAdapter* adapter_ = nullptr;  // Our Adapter. Read only.
  absl::flat_hash_set<BleV2Socket*> pending_sockets_ ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

// Container of operations that can be performed over the BLE medium.
class BleV2Medium : public api::ble_v2::BleMedium {
 public:
  explicit BleV2Medium(api::BluetoothAdapter& adapter);
  ~BleV2Medium() override;

  // Returns true once the Ble advertising has been initiated.
  bool StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_parameters) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAdvertising() override ABSL_LOCKS_EXCLUDED(mutex_);

  std::unique_ptr<AdvertisingSession> StartAdvertising(
      const api::ble_v2::BleAdvertisementData& advertising_data,
      api::ble_v2::AdvertiseParameters advertise_parameters,
      AdvertisingCallback callback) override ABSL_LOCKS_EXCLUDED(mutex_);

  bool StartScanning(const Uuid& service_uuid,
                     api::ble_v2::TxPowerLevel tx_power_level,
                     ScanCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopScanning() override ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<ScanningSession> StartScanning(
      const Uuid& service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
      ScanningCallback callback) override ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble_v2::GattServer> StartGattServer(
      api::ble_v2::ServerGattConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<api::ble_v2::GattClient> ConnectToGattServer(
      api::ble_v2::BlePeripheral& peripheral,
      api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::ClientGattConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Open server socket to listen for incoming connection.
  //
  // On success, returns a new BleServerSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::ble_v2::BleServerSocket> OpenServerSocket(
      const std::string& service_id) override ABSL_LOCKS_EXCLUDED(mutex_);

  // Connects to existing remote Ble peripheral.
  //
  // On success, returns a new BleSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::ble_v2::BleSocket> Connect(
      const std::string& service_id, api::ble_v2::TxPowerLevel tx_power_level,
      api::ble_v2::BlePeripheral& remote_peripheral,
      CancellationFlag* cancellation_flag) override ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsExtendedAdvertisementsAvailable() override;

  BluetoothAdapter& GetAdapter() { return *adapter_; }

  BleV2Peripheral& GetPeripheral() { return peripheral_; }

  bool GetRemotePeripheral(const std::string& mac_address,
                           GetRemotePeripheralCallback callback) override;

  bool GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                           GetRemotePeripheralCallback callback) override;

 private:
  class GattClient;
  // A concrete implementation for GattServer.
  class GattServer : public api::ble_v2::GattServer {
   public:
    GattServer(BleV2Medium& medium,
               api::ble_v2::ServerGattConnectionCallback callback);
    ~GattServer() override;

    api::ble_v2::BlePeripheral& GetBlePeripheral() override {
      return ble_peripheral_;
    }
    std::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
        const Uuid& service_uuid, const Uuid& characteristic_uuid,
        api::ble_v2::GattCharacteristic::Permission permission,
        api::ble_v2::GattCharacteristic::Property property) override;

    bool UpdateCharacteristic(
        const api::ble_v2::GattCharacteristic& characteristic,
        const nearby::ByteArray& value) override;

    absl::Status NotifyCharacteristicChanged(
        const api::ble_v2::GattCharacteristic& characteristic, bool confirm,
        const ByteArray& new_value) override;

    void Stop() override;
    bool IsStopped() { return stopped_; }
    bool DiscoverBleV2MediumGattCharacteristics(
        const Uuid& service_uuid,
        const std::vector<Uuid>& characteristic_uuids);

    absl::StatusOr<ByteArray> ReadCharacteristic(
        const BleV2Peripheral& remote_device,
        const api::ble_v2::GattCharacteristic& characteristic, int offset);

    absl::Status WriteCharacteristic(
        const BleV2Peripheral& remote_device,
        const api::ble_v2::GattCharacteristic& characteristic, int offset,
        absl::string_view data);

    bool AddCharacteristicSubscription(
        const BleV2Peripheral& remote_device,
        const api::ble_v2::GattCharacteristic& characteristic,
        absl::AnyInvocable<void(absl::string_view value)>);
    bool RemoveCharacteristicSubscription(
        const BleV2Peripheral& remote_device,
        const api::ble_v2::GattCharacteristic& characteristic);

    bool HasCharacteristic(
        const api::ble_v2::GattCharacteristic& characteristic);

    void Connect(GattClient* client);
    void Disconnect(GattClient* client);

   private:
    using SubscriberKey =
        std::pair<const BleV2Peripheral*, api::ble_v2::GattCharacteristic>;
    using SubscriberCallback =
        absl::AnyInvocable<void(absl::string_view value)>;
    absl::Mutex mutex_;
    BleV2Medium& medium_;
    api::ble_v2::ServerGattConnectionCallback callback_;
    BleV2Peripheral ble_peripheral_;
    absl::flat_hash_map<api::ble_v2::GattCharacteristic,
                        absl::StatusOr<ByteArray>>
        characteristics_ ABSL_GUARDED_BY(mutex_);
    absl::flat_hash_map<SubscriberKey, SubscriberCallback> subscribers_
        ABSL_GUARDED_BY(mutex_);
    std::vector<GattClient*> connected_clients_ ABSL_GUARDED_BY(mutex_);
    std::atomic_bool stopped_ = false;
    Lender<api::ble_v2::GattServer*> lender_{this};
  };

  // A concrete implementation for GattClient.
  class GattClient : public api::ble_v2::GattClient {
   public:
    GattClient(api::ble_v2::BlePeripheral& peripheral,
               Borrowable<api::ble_v2::GattServer*> gatt_server,
               api::ble_v2::ClientGattConnectionCallback callback);
    ~GattClient() override;
    bool DiscoverServiceAndCharacteristics(
        const Uuid& service_uuid,
        const std::vector<Uuid>& characteristic_uuids) override;

    std::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
        const Uuid& service_uuid, const Uuid& characteristic_uuid) override;

    std::optional<std::string> ReadCharacteristic(
        const api::ble_v2::GattCharacteristic& characteristic) override;

    bool WriteCharacteristic(
        const api::ble_v2::GattCharacteristic& characteristic,
        absl::string_view value,
        api::ble_v2::GattClient::WriteType write_type) override;

    bool SetCharacteristicSubscription(
        const api::ble_v2::GattCharacteristic& characteristic, bool enable,
        absl::AnyInvocable<void(absl::string_view value)>
            on_characteristic_changed_cb) override;

    void Disconnect() override;

    void OnServerDisconnected();

   private:
    absl::Mutex mutex_;

    // A flag to indicate the gatt connection alive or not. If it is
    // disconnected/*false*/, the instance needs to be created again to bring
    // it alive.
    std::atomic_bool is_connection_alive_ = true;
    BleV2Peripheral& peripheral_;
    Borrowable<api::ble_v2::GattServer*> gatt_server_;
    api::ble_v2::ClientGattConnectionCallback callback_;
  };

  bool IsStopped(Borrowable<api::ble_v2::GattServer*> server);
  absl::Mutex mutex_;
  BluetoothAdapter* adapter_;  // Our device adapter; read-only.
  BleV2Peripheral peripheral_{adapter_};
  absl::flat_hash_map<api::ble_v2::BlePeripheral::UniqueId,
                      std::unique_ptr<BleV2Peripheral>>
      remote_peripherals_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, BleV2ServerSocket*> server_sockets_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<std::pair<Uuid, std::uint32_t>>
      scanning_internal_session_ids_ ABSL_GUARDED_BY(mutex_);
  bool is_extended_advertisements_available_ = false;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_BLE_V2_H_
