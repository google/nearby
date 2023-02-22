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

#include <memory>

#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/windows/bluetooth_classic_device.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Networking.Sockets.h"
#include "winrt/Windows.Storage.Streams.h"

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

  explicit BluetoothSocket(StreamSocket streamSocket);

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
  // Returns true if successful, false otherwise
  bool Connect(HostName connectionHostName,
               winrt::hstring connectionServiceName);

  IAsyncAction CancelIOAsync();

 private:
  static constexpr int kMaxTransmitPacketSize = 4096;

  class BluetoothInputStream : public InputStream {
   public:
    explicit BluetoothInputStream(IInputStream stream);
    ~BluetoothInputStream() override = default;

    ExceptionOr<ByteArray> Read(std::int64_t size) override;
    Exception Close() override;

   private:
    IInputStream winrt_input_stream_{nullptr};
    Buffer read_buffer_{kMaxTransmitPacketSize};
  };

  class BluetoothOutputStream : public OutputStream {
   public:
    explicit BluetoothOutputStream(IOutputStream stream);
    ~BluetoothOutputStream() override = default;

    Exception Write(const ByteArray& data) override;
    Exception Flush() override;

    Exception Close() override;

   private:
    IOutputStream winrt_output_stream_{nullptr};
    Buffer write_buffer_{kMaxTransmitPacketSize};
  };

  winrt::fire_and_forget Listener_ConnectionStatusChanged(
      winrt::Windows::Devices::Bluetooth::BluetoothDevice device,
      winrt::Windows::Foundation::IInspectable const& args);

  StreamSocket windows_socket_{nullptr};
  bool is_bluetooth_socket_closed_ = false;
  BluetoothInputStream input_stream_{nullptr};
  BluetoothOutputStream output_stream_{nullptr};
  std::unique_ptr<BluetoothDevice> bluetooth_device_ = nullptr;
  winrt::Windows::Devices::Bluetooth::BluetoothDevice native_bluetooth_device_{
      nullptr};
  winrt::event_token connection_status_changed_token_{};
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SOCKET_H_
