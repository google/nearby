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

#ifndef PLATFORM_V2_API_BLUETOOTH_CLASSIC_H_
#define PLATFORM_V2_API_BLUETOOTH_CLASSIC_H_

#include <memory>
#include <string>

#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/exception.h"
#include "platform_v2/base/input_stream.h"
#include "platform_v2/base/output_stream.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace api {

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice {
 public:
  virtual ~BluetoothDevice() {}

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  virtual std::string GetName() const = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html.
class BluetoothSocket {
 public:
  virtual ~BluetoothSocket() {}

  // Returns the InputStream of the BluetoothSocket.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of the BluetoothSocket.
  virtual OutputStream& GetOutputStream() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
  virtual BluetoothDevice& GetRemoteDevice() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html.
class BluetoothServerSocket {
 public:
  virtual ~BluetoothServerSocket() {}

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
  //
  // returns Exception::kIo on error.
  virtual ExceptionOr<std::unique_ptr<BluetoothSocket>> Accept() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium {
 public:
  virtual ~BluetoothClassicMedium() {}

  class DiscoveryCallback {
   public:
    virtual ~DiscoveryCallback() {}

    // BluetoothDevice* is not owned by callbacks.
    // Pointer is guaranteed to remain valid for the duration of a call.
    virtual void OnDeviceDiscovered(BluetoothDevice* device) = 0;
    virtual void OnDeviceNameChanged(BluetoothDevice* device) = 0;
    virtual void OnDeviceLost(BluetoothDevice* device) = 0;
  };

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#startDiscovery()
  //
  // Returns true once the process of discovery has been initiated.
  //
  // Does not take ownership of the passed-in discovery_callback -- destroying
  // that is up to the caller.
  virtual bool StartDiscovery(const DiscoveryCallback& discovery_callback) = 0;
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#cancelDiscovery()
  //
  // Returns true once discovery is well and truly stopped; after this returns,
  // there must be no more invocations of the DiscoveryCallback passed in to
  // startDiscovery().
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
  // On success, returns a new BluetoothSocket, wrapped in a ExceptionOr object.
  // On error, returns Exception object.
  virtual ExceptionOr<std::unique_ptr<BluetoothSocket>> ConnectToService(
      BluetoothDevice* remote_device, absl::string_view service_uuid) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#listenUsingInsecureRfcommWithServiceRecord
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  //  Returns Exception::kIo on error.
  virtual ExceptionOr<std::unique_ptr<BluetoothServerSocket>> ListenForService(
      absl::string_view service_name, absl::string_view service_uuid) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_BLUETOOTH_CLASSIC_H_
