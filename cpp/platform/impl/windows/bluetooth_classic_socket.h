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

#ifndef PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SOCKET_H_
#define PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SOCKET_H_

#include "platform/impl/windows/generated/winrt/Windows.Foundation.h"
#include "platform/impl/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "platform/impl/windows/generated/winrt/Windows.Storage.Streams.h"

#include "platform/api/bluetooth_classic.h"

namespace location {
namespace nearby {
namespace windows {

// Provides data for a hostname or an IP address.
// https://docs.microsoft.com/en-us/uwp/api/windows.networking.hostname?view=winrt-20348
using winrt::Windows::Networking::HostName;

// Supports network communication using a stream socket over Bluetooth RFCOMM.
// https://docs.microsoft.com/en-us/uwp/api/windows.networking.sockets.streamsocket?view=winrt-20348
using winrt::Windows::Networking::Sockets::StreamSocket;

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html.
class BluetoothSocket : public api::BluetoothSocket {
 public:
  BluetoothSocket();
  // TODO(b/184975123): replace with real implementation.
  ~BluetoothSocket() override;

  // NOTE:
  // It is an undefined behavior if GetInputStream() or GetOutputStream() is
  // called for a not-connected BluetoothSocket, i.e. any object that is not
  // returned by BluetoothClassicMedium::ConnectToService() for client side or
  // BluetoothServerSocket::Accept() for server side of connection.

  // Returns the InputStream of this connected BluetoothSocket.
  // TODO(b/184975123): replace with real implementation.
  InputStream& GetInputStream() override;

  // Returns the OutputStream of this connected BluetoothSocket.
  // TODO(b/184975123): replace with real implementation.
  OutputStream& GetOutputStream() override;

  // Closes both input and output streams, marks Socket as closed.
  // After this call object should be treated as not connected.
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // TODO(b/184975123): replace with real implementation.
  Exception Close() override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
  // Returns valid BluetoothDevice pointer if there is a connection, and
  // nullptr otherwise.
  // TODO(b/184975123): replace with real implementation.
  api::BluetoothDevice* GetRemoteDevice() override;

  // Connect asynchronously to the target remote device
  winrt::Windows::Foundation::IAsyncAction ConnectAsync(
      HostName connectionHostName, winrt::hstring connectionServiceName);

 private:
  class FakeInputStream : public InputStream {
   public:
    ~FakeInputStream() override = default;
    ExceptionOr<ByteArray> Read(std::int64_t size) override {
      return ExceptionOr<ByteArray>(Exception::kFailed);
    }
    Exception Close() override { return {.value = Exception::kFailed}; }
  };
  class FakeOutputStream : public OutputStream {
   public:
    ~FakeOutputStream() override = default;

    Exception Write(const ByteArray& data) override {
      return {.value = Exception::kFailed};
    }
    Exception Flush() override { return {.value = Exception::kFailed}; }
    Exception Close() override { return {.value = Exception::kFailed}; }
  };
  FakeInputStream fake_input_stream_;
  FakeOutputStream fake_output_stream_;

  StreamSocket windows_socket_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SOCKET_H_
