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

#ifndef PLATFORM_IMPL_G3_BLUETOOTH_CLASSIC_H_
#define PLATFORM_IMPL_G3_BLUETOOTH_CLASSIC_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/g3/bluetooth_adapter.h"
#include "internal/platform/implementation/g3/socket_base.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace g3 {

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html.
class BluetoothSocket : public api::BluetoothSocket, public SocketBase {
 public:
  BluetoothSocket() = default;
  explicit BluetoothSocket(BluetoothAdapter* adapter) : adapter_(adapter) {}

  // Returns the InputStream of this connected BluetoothSocket.
  InputStream& GetInputStream() override {
    return SocketBase::GetInputStream();
  }

  // Returns the OutputStream of this connected BluetoothSocket.
  // This stream is for local side to write.
  OutputStream& GetOutputStream() override {
    return SocketBase::GetOutputStream();
  }

  // Closes both input and output streams, marks Socket as closed.
  // After this call object should be treated as not connected.
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override { return SocketBase::Close(); }

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
  // Returns valid BluetoothDevice pointer if there is a connection, and
  // nullptr otherwise.
  BluetoothDevice* GetRemoteDevice() override;

 private:
  BluetoothAdapter* adapter_ = nullptr;  // Our Adapter. Read only.
};

// https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html.
class BluetoothServerSocket : public api::BluetoothServerSocket {
 public:
  explicit BluetoothServerSocket(BluetoothAdapter& adapter)
      : adapter_(&adapter) {}
  ~BluetoothServerSocket() override;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  //
  // Called by the server side of a connection.
  // Returns BluetoothSocket to the server side.
  // If not null, returned socket is connected to its remote (client-side) peer.
  std::unique_ptr<api::BluetoothSocket> Accept() override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Blocks until either:
  // - connection is available, or
  // - server socket is closed, or
  // - error happens.
  //
  // Called by the client side of a connection.
  // socket is an initialized BluetoothSocket, associated with a client
  // BluetoothAdapter.
  // Returns true, if socket is successfully connected.
  bool Connect(BluetoothSocket& socket) ABSL_LOCKS_EXCLUDED(mutex_);

  // Called by the server side of a connection before passing ownership of
  // BluetoothServerSocker to user, to track validity of a pointer to this
  // server socket,
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // Calls close_notifier if it was previously set, and marks socket as closed.
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Exception DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  absl::Mutex mutex_;
  absl::CondVar cond_;
  BluetoothAdapter* adapter_ = nullptr;  // Our Adapter. Read only.
  absl::flat_hash_set<BluetoothSocket*> pending_sockets_
      ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

// A concrete implementation for BluetoothPairing.
class BluetoothPairing : public api::BluetoothPairing {
 public:
  explicit BluetoothPairing(api::BluetoothDevice& remote_device);
  BluetoothPairing(const BluetoothPairing&) = default;
  BluetoothPairing& operator=(const BluetoothPairing&) = default;
  ~BluetoothPairing() override;

  bool InitiatePairing(api::BluetoothPairingCallback pairing_cb) override;
  bool FinishPairing(std::optional<absl::string_view> pin_code) override;
  bool CancelPairing() override;
  bool Unpair() override;
  bool IsPaired() override;

 private:
  api::BluetoothDevice& remote_device_;
};

// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium : public api::BluetoothClassicMedium {
 public:
  explicit BluetoothClassicMedium(api::BluetoothAdapter& adapter);
  ~BluetoothClassicMedium() override;

  // NOTE(DiscoveryCallback):
  // BluetoothDevice is a proxy object created as a result of BT discovery.
  // Its lifetime spans between calls to device_discovered_cb and
  // device_lost_cb.
  // It is safe to use BluetoothDevice in device_discovered_cb() callback
  // and at any time afterwards, until device_lost_cb() is called.
  // It is not safe to use BluetoothDevice after returning from
  // device_lost_cb() callback.
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#startDiscovery()
  //
  // Returns true once the process of discovery has been initiated.
  bool StartDiscovery(DiscoveryCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#cancelDiscovery()
  //
  // Returns true once discovery is well and truly stopped; after this returns,
  // there must be no more invocations of the DiscoveryCallback passed in to
  // StartDiscovery().
  bool StopDiscovery() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Connects to existing remote BT service.
  //
  // A combination of
  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#createInsecureRfcommSocketToServiceRecord
  // followed by
  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#connect().
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  // On success, returns a new BluetoothSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::BluetoothSocket> ConnectToService(
      api::BluetoothDevice& remote_device, const std::string& service_uuid,
      CancellationFlag* cancellation_flag) override ABSL_LOCKS_EXCLUDED(mutex_);

  BluetoothAdapter& GetAdapter() { return *adapter_; }

  // Creates BT service, and begins listening for remote attempts to connect.
  //
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#listenUsingInsecureRfcommWithServiceRecord
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  //  Returns nullptr on error.
  std::unique_ptr<api::BluetoothServerSocket> ListenForService(
      const std::string& service_name, const std::string& service_uuid) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Return a Bluetooth pairing instance to handle the pairing process with the
  // remote device.
  std::unique_ptr<api::BluetoothPairing> CreatePairing(
      api::BluetoothDevice& remote_device) override;

  api::BluetoothDevice* GetRemoteDevice(
      const std::string& mac_address) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  absl::Mutex mutex_;
  BluetoothAdapter* adapter_;  // Our device adapter; read-only.
  absl::flat_hash_map<std::string, BluetoothServerSocket*> sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_BLUETOOTH_CLASSIC_H_
