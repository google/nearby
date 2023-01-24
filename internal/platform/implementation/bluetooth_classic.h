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

#ifndef PLATFORM_API_BLUETOOTH_CLASSIC_H_
#define PLATFORM_API_BLUETOOTH_CLASSIC_H_

#include <memory>
#include <string>

#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace api {

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice {
 public:
  virtual ~BluetoothDevice() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  virtual std::string GetName() const = 0;

  // Returns BT MAC address assigned to this device.
  virtual std::string GetMacAddress() const = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html.
class BluetoothSocket {
 public:
  virtual ~BluetoothSocket() = default;

  // NOTE:
  // It is an undefined behavior if GetInputStream() or GetOutputStream() is
  // called for a not-connected BluetoothSocket, i.e. any object that is not
  // returned by BluetoothClassicMedium::ConnectToService() for client side or
  // BluetoothServerSocket::Accept() for server side of connection.

  // Returns the InputStream of this connected BluetoothSocket.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of this connected BluetoothSocket.
  virtual OutputStream& GetOutputStream() = 0;

  // Closes both input and output streams, marks Socket as closed.
  // After this call object should be treated as not connected.
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
  // Returns valid BluetoothDevice pointer if there is a connection, and
  // nullptr otherwise.
  virtual BluetoothDevice* GetRemoteDevice() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html.
class BluetoothServerSocket {
 public:
  virtual ~BluetoothServerSocket() = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
  //
  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  virtual std::unique_ptr<BluetoothSocket> Accept() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium {
 public:
  virtual ~BluetoothClassicMedium() = default;

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

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#startDiscovery()
  //
  // Returns true once the process of discovery has been initiated.
  virtual bool StartDiscovery(DiscoveryCallback discovery_callback) = 0;
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#cancelDiscovery()
  //
  // Returns true once discovery is well and truly stopped; after this returns,
  // there must be no more invocations of the DiscoveryCallback passed in to
  // StartDiscovery().
  virtual bool StopDiscovery() = 0;

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
  virtual std::unique_ptr<BluetoothSocket> ConnectToService(
      BluetoothDevice& remote_device, const std::string& service_uuid,
      CancellationFlag* cancellation_flag) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#listenUsingInsecureRfcommWithServiceRecord
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  //  Returns nullptr error.
  virtual std::unique_ptr<BluetoothServerSocket> ListenForService(
      const std::string& service_name, const std::string& service_uuid) = 0;

  virtual BluetoothDevice* GetRemoteDevice(const std::string& mac_address) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_BLUETOOTH_CLASSIC_H_
