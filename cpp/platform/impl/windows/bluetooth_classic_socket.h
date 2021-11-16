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

#include "platform/api/bluetooth_classic.h"
#include "platform/impl/windows/bluetooth_classic_device.h"
#include "platform/impl/windows/generated/winrt/Windows.Foundation.h"
#include "platform/impl/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "platform/impl/windows/generated/winrt/Windows.Storage.Streams.h"

namespace location {
namespace nearby {
namespace windows {

// Provides data for a hostname or an IP address.
// https://docs.microsoft.com/en-us/uwp/api/windows.networking.hostname?view=winrt-20348
using winrt::Windows::Networking::HostName;

// Supports network communication using a stream socket over Bluetooth RFCOMM.
// https://docs.microsoft.com/en-us/uwp/api/windows.networking.sockets.streamsocket?view=winrt-20348
using winrt::Windows::Networking::Sockets::IStreamSocket;
using winrt::Windows::Networking::Sockets::StreamSocket;

// Provides a default implementation of the IBuffer interface and its related
// interfaces.
// https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.buffer?view=winrt-20348
using winrt::Windows::Storage::Streams::Buffer;

// Represents a sequential stream of bytes to be read.
// https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.iinputstream?view=winrt-20348
using winrt::Windows::Storage::Streams::IInputStream;

// Represents a sequential stream of bytes to be written.
// https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.ioutputstream?view=winrt-20348
using winrt::Windows::Storage::Streams::IOutputStream;

// Specifies the read options for an input stream.
// This enumeration has a FlagsAttribute attribute that allows a bitwise
// combination of its member values.
// https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.inputstreamoptions?view=winrt-20348
using winrt::Windows::Storage::Streams::InputStreamOptions;

// Reads data from an input stream.
// https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.datareader?view=winrt-20348
using winrt::Windows::Storage::Streams::DataReader;

// Represents an asynchronous action.
// https://docs.microsoft.com/en-us/uwp/api/windows.foundation.iasyncaction?view=winrt-20348
using winrt::Windows::Foundation::IAsyncAction;

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html.
class BluetoothSocket : public api::BluetoothSocket {
 public:
  BluetoothSocket();

  BluetoothSocket(StreamSocket streamSocket);

  ~BluetoothSocket() override;

  // NOTE:
  // It is an undefined behavior if GetInputStream() or GetOutputStream() is
  // called for a not-connected BluetoothSocket, i.e. any object that is not
  // returned by BluetoothClassicMedium::ConnectToService() for client side or
  // BluetoothServerSocket::Accept() for server side of connection.

  // Returns the InputStream of this connected BluetoothSocket.
  InputStream& GetInputStream() override;

  // Returns the OutputStream of this connected BluetoothSocket.
  OutputStream& GetOutputStream() override;

  // Closes both input and output streams, marks Socket as closed.
  // After this call object should be treated as not connected.
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
  // Returns valid BluetoothDevice pointer if there is a connection, and
  // nullptr otherwise.
  api::BluetoothDevice* GetRemoteDevice() override;

  // Connect asynchronously to the target remote device
  void Connect(HostName connectionHostName,
                            winrt::hstring connectionServiceName);

  IAsyncAction CancelIOAsync();

 private:
  class BluetoothInputStream : public InputStream {
   public:
    BluetoothInputStream(IInputStream stream);
    ~BluetoothInputStream() override = default;

    ExceptionOr<ByteArray> Read(std::int64_t size) override;
    Exception Close() override;

   private:
    IInputStream winrt_stream_;
  };

  class BluetoothOutputStream : public OutputStream {
   public:
    BluetoothOutputStream(IOutputStream stream);
    ~BluetoothOutputStream() override = default;

    Exception Write(const ByteArray& data) override;
    Exception Flush() override;

    Exception Close() override;

   private:
    IOutputStream winrt_stream_;
  };

  std::unique_ptr<BluetoothInputStream> input_stream_;
  std::unique_ptr<BluetoothOutputStream> output_stream_;

  StreamSocket windows_socket_;
  std::unique_ptr<BluetoothDevice> bluetooth_device_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SOCKET_H_
