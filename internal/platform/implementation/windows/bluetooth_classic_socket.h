// Copyright 2020-2023 Google LLC
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

#include <windows.h>

#include <cstdint>
#include <memory>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/windows/bluetooth_classic_device.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Storage.Streams.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace windows {

class BluetoothSocket : public api::BluetoothSocket {
 public:
  BluetoothSocket();
  explicit BluetoothSocket(
      ::winrt::Windows::Networking::Sockets::StreamSocket stream_socket);
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
  bool Connect(::winrt::Windows::Networking::HostName connection_host_name,
               ::winrt::hstring connection_service_name);

 private:
  static constexpr int kInitialTransmitPacketSize = 4096;

  class BluetoothInputStream : public InputStream {
   public:
    explicit BluetoothInputStream(
        ::winrt::Windows::Storage::Streams::IInputStream stream);
    ~BluetoothInputStream() override = default;

    ExceptionOr<ByteArray> Read(std::int64_t size) override;
    Exception Close() override;

   private:
    ::winrt::Windows::Storage::Streams::IInputStream winrt_input_stream_{
        nullptr};
    ::winrt::Windows::Storage::Streams::Buffer read_buffer_{
        kInitialTransmitPacketSize};
  };

  class BluetoothOutputStream : public OutputStream {
   public:
    explicit BluetoothOutputStream(
        ::winrt::Windows::Storage::Streams::IOutputStream stream);
    ~BluetoothOutputStream() override = default;

    Exception Write(const ByteArray& data) override;
    Exception Flush() override;

    Exception Close() override;

   private:
    ::winrt::Windows::Storage::Streams::IOutputStream winrt_output_stream_{
        nullptr};
    ::winrt::Windows::Storage::Streams::Buffer write_buffer_{
        kInitialTransmitPacketSize};
  };

  bool InternalConnect(
      ::winrt::Windows::Networking::HostName connection_host_name,
      ::winrt::hstring connection_service_name);

  ::winrt::fire_and_forget Listener_ConnectionStatusChanged(
      ::winrt::Windows::Devices::Bluetooth::BluetoothDevice device,
      ::winrt::Windows::Foundation::IInspectable const& args);

  ::winrt::Windows::Networking::Sockets::StreamSocket windows_socket_{nullptr};
  bool is_bluetooth_socket_closed_ = false;
  BluetoothInputStream input_stream_{nullptr};
  BluetoothOutputStream output_stream_{nullptr};
  std::unique_ptr<BluetoothDevice> bluetooth_device_ = nullptr;
  ::winrt::Windows::Devices::Bluetooth::BluetoothDevice
      native_bluetooth_device_{nullptr};
  ::winrt::event_token connection_status_changed_token_{};
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SOCKET_H_
