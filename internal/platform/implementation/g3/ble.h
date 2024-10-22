// Copyright 2020 Google LLC
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

#ifndef PLATFORM_IMPL_G3_BLE_H_
#define PLATFORM_IMPL_G3_BLE_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/escaping.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/g3/bluetooth_adapter.h"
#include "internal/platform/implementation/g3/bluetooth_classic.h"
#include "internal/platform/implementation/g3/multi_thread_executor.h"
#include "internal/platform/implementation/g3/socket_base.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace g3 {

class BleMedium;

class BleSocket : public api::BleSocket, public SocketBase {
 public:
  BleSocket() = default;
  explicit BleSocket(BlePeripheral* peripheral) : peripheral_(peripheral) {}

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
  BleSocket* GetRemoteSocket() {
    return static_cast<BleSocket*>(SocketBase::GetRemoteSocket());
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override { return SocketBase::Close(); }

  // Returns valid BlePeripheral pointer if there is a connection, and
  // nullptr otherwise.
  BlePeripheral* GetRemotePeripheral() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  BlePeripheral* peripheral_;
};

class BleServerSocket {
 public:
  ~BleServerSocket();

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
  std::unique_ptr<api::BleSocket> Accept(BlePeripheral* peripheral)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Blocks until either:
  // - connection is available, or
  // - server socket is closed, or
  // - error happens.
  //
  // Called by the client side of a connection.
  // Returns true, if socket is successfully connected.
  bool Connect(BleSocket& socket) ABSL_LOCKS_EXCLUDED(mutex_);

  // Called by the server side of a connection before passing ownership of
  // BleServerSocker to user, to track validity of a pointer to this
  // server socket,
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // Calls close_notifier if it was previously set, and marks socket as closed.
  Exception Close() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Exception DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  absl::Mutex mutex_;
  absl::CondVar cond_;
  absl::flat_hash_set<BleSocket*> pending_sockets_ ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

// Container of operations that can be performed over the BLE medium.
class BleMedium : public api::BleMedium {
 public:
  explicit BleMedium(api::BluetoothAdapter& adapter);
  ~BleMedium() override;

  // Returns true once the Ble advertising has been initiated.
  bool StartAdvertising(
      const std::string& service_id, const ByteArray& advertisement_bytes,
      const std::string& fast_advertisement_service_uuid) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAdvertising(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once the Ble scanning has been initiated.
  bool StartScanning(const std::string& service_id,
                     const std::string& fast_advertisement_service_uuid,
                     DiscoveredPeripheralCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once Ble scanning for service_id is well and truly
  // stopped; after this returns, there must be no more invocations of the
  // DiscoveredPeripheralCallback passed in to StartScanning() for service_id.
  bool StopScanning(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true once Ble socket connection requests to service_id can be
  // accepted.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAcceptingConnections(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Connects to existing remote Ble peripheral.
  //
  // On success, returns a new BleSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::BleSocket> Connect(
      api::BlePeripheral& remote_peripheral, const std::string& service_id,
      CancellationFlag* cancellation_flag) override ABSL_LOCKS_EXCLUDED(mutex_);

  BluetoothAdapter& GetAdapter() { return *adapter_; }

 private:
  static constexpr int kMaxConcurrentAcceptLoops = 5;

  struct AdvertisingInfo {
    bool Empty() const { return service_id.empty(); }
    void Clear() { service_id.clear(); }

    std::string service_id;
  };

  struct ScanningInfo {
    bool Empty() const { return service_id.empty(); }
    void Clear() { service_id.clear(); }

    std::string service_id;
  };

  absl::Mutex mutex_;
  BluetoothAdapter* adapter_;  // Our device adapter; read-only.

  // A thread pool dedicated to running all the accept loops from
  // StartAdvertising().
  MultiThreadExecutor accept_loops_runner_{kMaxConcurrentAcceptLoops};
  std::atomic_bool acceptance_thread_running_ = false;

  // A thread pool dedicated to wait to complete the accept_loops_runner_.
  MultiThreadExecutor close_accept_loops_runner_{1};

  // A server socket is established when start advertising.
  std::unique_ptr<BleServerSocket> server_socket_;
  AdvertisingInfo advertising_info_ ABSL_GUARDED_BY(mutex_);
  ScanningInfo scanning_info_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_BLE_H_
