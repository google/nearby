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

#include "platform/api/input_stream.h"
#include "platform/api/output_stream.h"
#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice {
 public:
  virtual ~BluetoothDevice() {}

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  virtual std::string getName() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html.
class BluetoothSocket {
 public:
  virtual ~BluetoothSocket() {}

  // Returns the InputStream of the BluetoothSocket, or a null Ptr<InputStream>
  // on error.
  //
  // The returned Ptr is not owned by the caller, and can be invalidated once
  // the BluetoothSocket object is destroyed.
  virtual Ptr<InputStream> getInputStream() = 0;

  // Returns the OutputStream of the BluetoothSocket, or a null
  // Ptr<OutputStream> on error.
  //
  // The returned Ptr is not owned by the caller, and can be invalidated once
  // the BluetoothSocket object is destroyed.
  virtual Ptr<OutputStream> getOutputStream() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#close()
  //
  // Returns Exception::IO on error, Exception::NONE otherwise.
  virtual Exception::Value close() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
  //
  // The returned Ptr is not owned by the caller, and can be invalidated once
  // the BluetoothSocket object is destroyed.
  virtual Ptr<BluetoothDevice> getRemoteDevice() = 0;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html.
class BluetoothServerSocket {
 public:
  virtual ~BluetoothServerSocket() {}

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
  //
  // The returned Ptr will be owned (and destroyed) by the caller. Returns
  // Exception::IO on error.
  virtual ExceptionOr<Ptr<BluetoothSocket>> accept() = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
  //
  // Returns Exception::IO on error, Exception::NONE otherwise.
  virtual Exception::Value close() = 0;
};

// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium {
 public:
  virtual ~BluetoothClassicMedium() {}

  class DiscoveryCallback {
   public:
    virtual ~DiscoveryCallback() {}

    // The Ptrs provided in these callback methods will be owned (and
    // destroyed) by the recipient of the callback methods (i.e. the creator of
    // the concrete DiscoveryCallback object).
    virtual void onDeviceDiscovered(Ptr<BluetoothDevice> device) = 0;
    virtual void onDeviceNameChanged(Ptr<BluetoothDevice> device) = 0;
    virtual void onDeviceLost(Ptr<BluetoothDevice> device) = 0;
  };

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#startDiscovery()
  //
  // Returns true once the process of discovery has been initiated.
  //
  // Does not take ownership of the passed-in discovery_callback -- destroying
  // that is up to the caller.
  virtual bool startDiscovery(Ptr<DiscoveryCallback> discovery_callback) = 0;
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#cancelDiscovery()
  //
  // Returns true once discovery is well and truly stopped; after this returns,
  // there must be no more invocations of the DiscoveryCallback passed in to
  // startDiscovery().
  //
  // Does not need to bother with destroying the DiscoveryCallback passed in to
  // startDiscovery() -- that's the job of the caller.
  virtual bool stopDiscovery() = 0;

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
  // The returned Ptr will be owned (and destroyed) by the caller. Returns
  // Exception::IO on error.
  virtual ExceptionOr<Ptr<BluetoothSocket>> connectToService(
      Ptr<BluetoothDevice> remote_device,
      const std::string& service_uuid) = 0;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#listenUsingInsecureRfcommWithServiceRecord
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  // The returned Ptr will be owned (and destroyed) by the caller. Returns
  // Exception::IO on error.
  virtual ExceptionOr<Ptr<BluetoothServerSocket>> listenForService(
      const std::string& service_name,
      const std::string& service_uuid) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_BLUETOOTH_CLASSIC_H_
