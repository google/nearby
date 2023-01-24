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

#ifndef PLATFORM_PUBLIC_BLUETOOTH_CLASSIC_H_
#define PLATFORM_PUBLIC_BLUETOOTH_CLASSIC_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/output_stream.h"

namespace nearby {

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html.
class BluetoothSocket final {
 public:
  BluetoothSocket() = default;
  BluetoothSocket(const BluetoothSocket&) = default;
  BluetoothSocket& operator=(const BluetoothSocket&) = default;
  explicit BluetoothSocket(std::unique_ptr<api::BluetoothSocket> socket)
      : impl_(socket.release()) {}
  ~BluetoothSocket() = default;

  // Returns the InputStream of this connected BluetoothSocket.
  InputStream& GetInputStream() { return impl_->GetInputStream(); }

  // Returns the OutputStream of this connected BluetoothSocket.
  OutputStream& GetOutputStream() { return impl_->GetOutputStream(); }

  // Closes both input and output streams, marks Socket as closed.
  // After this call object should be treated as not connected.
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() { return impl_->Close(); }

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
  BluetoothDevice GetRemoteDevice() {
    return BluetoothDevice(impl_->GetRemoteDevice());
  }

  // Returns true if a socket is usable. If this method returns false,
  // it is not safe to call any other method.
  // NOTE(socket validity):
  // Socket created by a default public constructor is not valid, because
  // it is missing platform implementation.
  // The only way to obtain a valid socket is through connection, such as
  // an object returned by either BluetoothClassicMedium::ConnectTotService or
  // BluetoothServerSocket::Accept().
  // These methods may also return an invalid socket if connection failed for
  // any reason.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  // Returned reference will remain valid for while BluetoothSocket object is
  // itself valid. Typically BluetoothSocket lifetime matches duration of the
  // connection, and is controlled by end user, since they hold the instance.
  api::BluetoothSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::BluetoothSocket> impl_;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html.
class BluetoothServerSocket final {
 public:
  BluetoothServerSocket() = default;
  BluetoothServerSocket(const BluetoothServerSocket&) = default;
  BluetoothServerSocket& operator=(const BluetoothServerSocket&) = default;
  ~BluetoothServerSocket() = default;
  explicit BluetoothServerSocket(
      std::unique_ptr<api::BluetoothServerSocket> socket)
      : impl_(std::move(socket)) {}

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
  //
  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  BluetoothSocket Accept() {
    auto socket = impl_->Accept();
    if (!socket) {
      NEARBY_LOGS(INFO) << "Accept() failed on server socket: " << this;
    }
    return BluetoothSocket(std::move(socket));
  }

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    NEARBY_LOGS(INFO) << "Closing server socket: " << this;
    return impl_->Close();
  }

  bool IsValid() const { return impl_ != nullptr; }
  api::BluetoothServerSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::BluetoothServerSocket> impl_;
};

// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium final {
 public:
  using Platform = api::ImplementationPlatform;
  struct DiscoveryCallback {
    // BluetoothDevice is a proxy object created as a result of BT discovery.
    // Its lifetime spans between calls to device_discovered_cb and
    // device_lost_cb.
    // It is safe to use BluetoothDevice in device_discovered_cb() callback
    // and at any time afterwards, until device_lost_cb() is called.
    // It is not safe to use BluetoothDevice after returning from
    // device_lost_cb() callback.
    absl::AnyInvocable<void(BluetoothDevice& device)> device_discovered_cb =
        DefaultCallback<BluetoothDevice&>();
    absl::AnyInvocable<void(BluetoothDevice& device)> device_name_changed_cb =
        DefaultCallback<BluetoothDevice&>();
    absl::AnyInvocable<void(BluetoothDevice& device)> device_lost_cb =
        DefaultCallback<BluetoothDevice&>();
  };
  struct DeviceDiscoveryInfo {
    BluetoothDevice device;
  };

  explicit BluetoothClassicMedium(BluetoothAdapter& adapter)
      : impl_(Platform::CreateBluetoothClassicMedium(adapter.GetImpl())),
        adapter_(adapter) {}

  ~BluetoothClassicMedium();

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
  bool StartDiscovery(DiscoveryCallback callback);

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#cancelDiscovery()
  //
  // Returns true once discovery is well and truly stopped; after this returns,
  // there must be no more invocations of the DiscoveryCallback passed in to
  // StartDiscovery().
  bool StopDiscovery();

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
  // Returns a new BluetoothSocket. On Success, BluetoothSocket::IsValid()
  // returns true.
  BluetoothSocket ConnectToService(BluetoothDevice& remote_device,
                                   const std::string& service_uuid,
                                   CancellationFlag* cancellation_flag);

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#listenUsingInsecureRfcommWithServiceRecord
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  // Returns a new BluetoothServerSocket.
  // On Success, BluetoothServerSocket::IsValid() returns true.
  BluetoothServerSocket ListenForService(const std::string& service_name,
                                         const std::string& service_uuid) {
    return BluetoothServerSocket(
        impl_->ListenForService(service_name, service_uuid));
  }

  bool IsValid() const { return impl_ != nullptr; }

  api::BluetoothClassicMedium& GetImpl() { return *impl_; }
  BluetoothAdapter& GetAdapter() { return adapter_; }
  std::string GetMacAddress() const { return adapter_.GetMacAddress(); }
  BluetoothDevice GetRemoteDevice(const std::string& mac_address) {
    return BluetoothDevice(impl_->GetRemoteDevice(mac_address));
  }

 private:
  Mutex mutex_;
  std::unique_ptr<api::BluetoothClassicMedium> impl_;
  BluetoothAdapter& adapter_;
  absl::flat_hash_map<api::BluetoothDevice*,
                      std::unique_ptr<DeviceDiscoveryInfo>>
      devices_ ABSL_GUARDED_BY(mutex_);
  DiscoveryCallback discovery_callback_ ABSL_GUARDED_BY(mutex_);
  bool discovery_enabled_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_BLUETOOTH_CLASSIC_H_
