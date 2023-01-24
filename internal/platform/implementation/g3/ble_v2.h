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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/g3/bluetooth_adapter.h"
#include "internal/platform/implementation/g3/pipe.h"
#include "internal/platform/prng.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace g3 {

class BleV2nMedium;

class BleV2Socket : public api::ble_v2::BleSocket {
 public:
  explicit BleV2Socket(BluetoothAdapter* adapter) : adapter_(adapter) {}
  BleV2Socket(const BleV2Socket&) = default;
  BleV2Socket& operator=(const BleV2Socket&) = default;
  BleV2Socket(BleV2Socket&&) = default;
  BleV2Socket& operator=(BleV2Socket&&) = default;
  ~BleV2Socket() override;

  // Connect to another BleSocket, to form a functional low-level channel.
  // from this point on, and until Close is called, connection exists.
  void Connect(BleV2Socket& other) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the InputStream of this connected BleSocket.
  InputStream& GetInputStream() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the OutputStream of this connected BleSocket.
  // This stream is for local side to write.
  OutputStream& GetOutputStream() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns address of a remote BleSocket or nullptr.
  BleV2Socket* GetRemoteSocket() ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if connection exists to the (possibly closed) remote socket.
  bool IsConnected() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if socket is closed.
  bool IsClosed() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns valid BlePeripheral pointer if there is a connection, and
  // nullptr otherwise.
  BleV2Peripheral* GetRemotePeripheral() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  void DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns true if connection exists to the (possibly closed) remote socket.
  bool IsConnectedLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Returns InputStream of our side of a connection.
  // This is what the remote side is supposed to read from.
  // This is a helper for GetInputStream() method.
  InputStream& GetLocalInputStream() ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns OutputStream of our side of a connection.
  // This is what the local size is supposed to write to.
  // This is a helper for GetOutputStream() method.
  OutputStream& GetLocalOutputStream() ABSL_LOCKS_EXCLUDED(mutex_);

  // Output pipe is initialized by constructor, it remains always valid, until
  // it is closed. it represents output part of a local socket. Input part of a
  // local socket comes from the peer socket, after connection.
  std::shared_ptr<Pipe> output_{new Pipe};
  std::shared_ptr<Pipe> input_;
  mutable absl::Mutex mutex_;
  BluetoothAdapter* adapter_ = nullptr;  // Our Adapter. Read only.
  BleV2Socket* remote_socket_ ABSL_GUARDED_BY(mutex_) = nullptr;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
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

 private:
  // A concrete implementation for GattServer.
  class GattServer : public api::ble_v2::GattServer {
   public:
    std::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
        const Uuid& service_uuid, const Uuid& characteristic_uuid,
        const std::vector<api::ble_v2::GattCharacteristic::Permission>&
            permissions,
        const std::vector<api::ble_v2::GattCharacteristic::Property>&
            properties) override;

    bool UpdateCharacteristic(
        const api::ble_v2::GattCharacteristic& characteristic,
        const nearby::ByteArray& value) override;

    void Stop() override;
  };

  // A concrete implementation for GattClient.
  class GattClient : public api::ble_v2::GattClient {
   public:
    bool DiscoverServiceAndCharacteristics(
        const Uuid& service_uuid,
        const std::vector<Uuid>& characteristic_uuids) override;

    std::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
        const Uuid& service_uuid, const Uuid& characteristic_uuid) override;

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
  absl::flat_hash_map<std::string, BleV2ServerSocket*> server_sockets_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<std::pair<Uuid, std::uint32_t>>
      scanning_internal_session_ids_ ABSL_GUARDED_BY(mutex_);
  // TODO(edwinwu): Adds extended advertisement for testing.
  bool is_support_extended_advertisement_ = false;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_BLE_V2_H_
