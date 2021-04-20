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

#ifndef PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_H_
#define PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_H_

#include "platform/api/bluetooth_classic.h"
#include "platform/base/exception.h"
#include "platform/base/input_stream.h"
#include "platform/base/output_stream.h"
namespace location {
namespace nearby {
namespace windows {

// https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html.
class BluetoothDevice : public api::BluetoothDevice {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~BluetoothDevice() override = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getName()
  // TODO(b/184975123): replace with real implementation.
  std::string GetName() const override { return "Un-implemented"; }

  // Returns BT MAC address assigned to this device.
  // TODO(b/184975123): replace with real implementation.
  std::string GetMacAddress() const override { return "Un-implemented"; }
};

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html.
class BluetoothSocket : public api::BluetoothSocket {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~BluetoothSocket() override;

  // NOTE:
  // It is an undefined behavior if GetInputStream() or GetOutputStream() is
  // called for a not-connected BluetoothSocket, i.e. any object that is not
  // returned by BluetoothClassicMedium::ConnectToService() for client side or
  // BluetoothServerSocket::Accept() for server side of connection.

  // Returns the InputStream of this connected BluetoothSocket.
  // TODO(b/184975123): replace with real implementation.
  InputStream& GetInputStream() override { return fake_input_stream_; }

  // Returns the OutputStream of this connected BluetoothSocket.
  // TODO(b/184975123): replace with real implementation.
  OutputStream& GetOutputStream() override { return fake_output_stream_; }

  // Closes both input and output streams, marks Socket as closed.
  // After this call object should be treated as not connected.
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // TODO(b/184975123): replace with real implementation.
  Exception Close() override { return Exception{}; }

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
  // Returns valid BluetoothDevice pointer if there is a connection, and
  // nullptr otherwise.
  // TODO(b/184975123): replace with real implementation.
  BluetoothDevice* GetRemoteDevice() override { return nullptr; }

 private:
  class FakeInputStream : public InputStream {
    ~FakeInputStream() override = default;
    ExceptionOr<ByteArray> Read(std::int64_t size) override {
      return ExceptionOr<ByteArray>(Exception::kFailed);
    }
    Exception Close() override { return {.value = Exception::kFailed}; }
  };
  class FakeOutputStream : public OutputStream {
    ~FakeOutputStream() override = default;

    Exception Write(const ByteArray& data) override {
      return {.value = Exception::kFailed};
    }
    Exception Flush() override { return {.value = Exception::kFailed}; }
    Exception Close() override { return {.value = Exception::kFailed}; }
  };
  FakeInputStream fake_input_stream_;
  FakeOutputStream fake_output_stream_;
};

// https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html.
class BluetoothServerSocket : public api::BluetoothServerSocket {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~BluetoothServerSocket() override = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
  //
  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  // TODO(b/184975123): replace with real implementation.
  std::unique_ptr<api::BluetoothSocket> Accept() override { return nullptr; }

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // TODO(b/184975123): replace with real implementation.
  Exception Close() override { return Exception{}; }
};

// Container of operations that can be performed over the Bluetooth Classic
// medium.
class BluetoothClassicMedium : public api::BluetoothClassicMedium {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~BluetoothClassicMedium() override = default;

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#startDiscovery()
  //
  // Returns true once the process of discovery has been initiated.
  // TODO(b/184975123): replace with real implementation.
  bool StartDiscovery(DiscoveryCallback discovery_callback) override {
    return false;
  }
  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#cancelDiscovery()
  //
  // Returns true once discovery is well and truly stopped; after this returns,
  // there must be no more invocations of the DiscoveryCallback passed in to
  // StartDiscovery().
  // TODO(b/184975123): replace with real implementation.
  bool StopDiscovery() override { return false; }

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
  // TODO(b/184975123): replace with real implementation.
  std::unique_ptr<api::BluetoothSocket> ConnectToService(
      api::BluetoothDevice& remote_device, const std::string& service_uuid,
      CancellationFlag* cancellation_flag) override {
    return nullptr;
  }

  // https://developer.android.com/reference/android/bluetooth/BluetoothAdapter.html#listenUsingInsecureRfcommWithServiceRecord
  //
  // service_uuid is the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of a
  // type 3 name-based
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
  // UUID.
  //
  //  Returns nullptr error.
  // TODO(b/184975123): replace with real implementation.
  std::unique_ptr<api::BluetoothServerSocket> ListenForService(
      const std::string& service_name,
      const std::string& service_uuid) override {
    return nullptr;
  }

  // TODO(b/184975123): replace with real implementation.
  BluetoothDevice* GetRemoteDevice(const std::string& mac_address) override {
    return nullptr;
  }
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_H_
