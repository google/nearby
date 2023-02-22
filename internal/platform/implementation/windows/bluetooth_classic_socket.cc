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

#include "internal/platform/implementation/windows/bluetooth_classic_socket.h"

#include <cstring>
#include <exception>
#include <memory>
#include <utility>

#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Networking.Sockets.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {
namespace {
using ::winrt::Windows::Devices::Bluetooth::BluetoothConnectionStatus;
}

BluetoothSocket::BluetoothSocket(StreamSocket streamSocket)
    : windows_socket_(streamSocket) {
  NEARBY_LOGS(INFO) << __func__ << ": Initialize bluetooth socket.";
  native_bluetooth_device_ =
      winrt::Windows::Devices::Bluetooth::BluetoothDevice::FromHostNameAsync(
          windows_socket_.Information().RemoteHostName())
          .get();
  if (FeatureFlags::GetInstance()
          .GetFlags()
          .enable_bluetooth_connection_status_track) {
    NEARBY_LOGS(INFO)
        << "Flag enable_bluetooth_connection_status_track is enabled.";
    connection_status_changed_token_ =
        native_bluetooth_device_.ConnectionStatusChanged(
            {this, &BluetoothSocket::Listener_ConnectionStatusChanged});
  }

  bluetooth_device_ =
      std::make_unique<BluetoothDevice>(native_bluetooth_device_);

  input_stream_ = BluetoothInputStream(windows_socket_.InputStream());
  output_stream_ = BluetoothOutputStream(windows_socket_.OutputStream());
}

BluetoothSocket::BluetoothSocket() {}

BluetoothSocket::~BluetoothSocket() { Close(); }

// NOTE:
// It is an undefined behavior if GetInputStream() or GetOutputStream() is
// called for a not-connected BluetoothSocket, i.e. any object that is not
// returned by BluetoothClassicMedium::ConnectToService() for client side or
// BluetoothServerSocket::Accept() for server side of connection.
// Returns the InputStream of this connected BluetoothSocket.
InputStream& BluetoothSocket::GetInputStream() { return input_stream_; }

// Returns the OutputStream of this connected BluetoothSocket.
OutputStream& BluetoothSocket::GetOutputStream() { return output_stream_; }

// Closes both input and output streams, marks Socket as closed.
// After this call object should be treated as not connected.
// Returns Exception::kIo on error, Exception::kSuccess otherwise.
Exception BluetoothSocket::Close() {
  NEARBY_LOGS(INFO) << __func__ << ": Close bluetooth socket.";

  // The Close method aborts any pending operations and releases all unmanaged
  // resources associated with the StreamSocket object, including the Input and
  // Output streams
  try {
    if (is_bluetooth_socket_closed_ || native_bluetooth_device_ == nullptr) {
      return {Exception::kSuccess};
    }

    if (windows_socket_ != nullptr) {
      windows_socket_.Close();
    }

    if (FeatureFlags::GetInstance()
            .GetFlags()
            .enable_bluetooth_connection_status_track) {
      native_bluetooth_device_.ConnectionStatusChanged(
          connection_status_changed_token_);
    }

    native_bluetooth_device_.Close();
    is_bluetooth_socket_closed_ = true;
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
// Returns valid BluetoothDevice pointer if there is a connection, and
// nullptr otherwise.
api::BluetoothDevice* BluetoothSocket::GetRemoteDevice() {
  return bluetooth_device_.get();
}

// Starts an asynchronous operation on a StreamSocket object to connect to a
// remote network destination specified by a remote hostname and a remote
// service name.
bool BluetoothSocket::Connect(HostName connectionHostName,
                              winrt::hstring connectionServiceName) {
  try {
    if (connectionHostName == nullptr || connectionServiceName.empty()) {
      NEARBY_LOGS(ERROR)
          << __func__
          << ": Bluetooth socket connection failed. Attempting to "
             "connect to empty HostName/MAC address or ServiceName.";
      return false;
    }

    NEARBY_LOGS(INFO) << __func__
                      << ": Bluetooth socket connection to host name:"
                      << winrt::to_string(connectionHostName.DisplayName())
                      << ", service name:"
                      << winrt::to_string(connectionServiceName);

    windows_socket_ = winrt::Windows::Networking::Sockets::StreamSocket();

    // https://docs.microsoft.com/en-us/uwp/api/windows.networking.sockets.streamsocket.connectasync?view=winrt-20348
    windows_socket_.ConnectAsync(connectionHostName, connectionServiceName)
        .get();

    auto info = windows_socket_.Information();
    auto hostName = info.RemoteHostName();

    native_bluetooth_device_ =
        winrt::Windows::Devices::Bluetooth::BluetoothDevice::FromHostNameAsync(
            windows_socket_.Information().RemoteHostName())
            .get();

    if (FeatureFlags::GetInstance()
            .GetFlags()
            .enable_bluetooth_connection_status_track) {
      NEARBY_LOGS(INFO)
          << "Flag enable_bluetooth_connection_status_track is enabled.";
      connection_status_changed_token_ =
          native_bluetooth_device_.ConnectionStatusChanged(
              {this, &BluetoothSocket::Listener_ConnectionStatusChanged});
    }

    bluetooth_device_ =
        std::make_unique<BluetoothDevice>(native_bluetooth_device_);

    input_stream_ = BluetoothInputStream(windows_socket_.InputStream());
    output_stream_ = BluetoothOutputStream(windows_socket_.OutputStream());

    NEARBY_LOGS(INFO) << __func__
                      << ": Bluetooth socket successfully connected to "
                      << bluetooth_device_->GetName();
    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return false;
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return false;
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return false;
  }
}

BluetoothSocket::BluetoothInputStream::BluetoothInputStream(
    IInputStream stream) {
  winrt_input_stream_ = stream;
}

ExceptionOr<ByteArray> BluetoothSocket::BluetoothInputStream::Read(
    std::int64_t size) {
  try {
    if (size <= 0 || size > kMaxTransmitPacketSize) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Invalid transmit packet size: " << size;
      return {Exception::kIo};
    }

    // Init the read buffer.
    read_buffer_.Length(0);

    auto ibuffer = winrt_input_stream_
                       .ReadAsync(read_buffer_, size, InputStreamOptions::None)
                       .get();

    if (ibuffer.Length() != size) {
      NEARBY_LOGS(WARNING) << __func__ << ": Got " << ibuffer.Length()
                           << " bytes of total " << size << " bytes.";
    }

    ByteArray data((char*)ibuffer.data(), ibuffer.Length());
    return ExceptionOr(data);
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

IAsyncAction BluetoothSocket::CancelIOAsync() {
  // Cancels pending reads and writes over a StreamSocket object.
  // https://docs.microsoft.com/en-us/uwp/api/windows.networking.sockets.streamsocket.cancelioasync?view=winrt-20348
  return windows_socket_.CancelIOAsync();
}

Exception BluetoothSocket::BluetoothInputStream::Close() {
  NEARBY_LOGS(INFO) << __func__ << ": Close bluetooth input stream.";

  try {
    if (winrt_input_stream_ != nullptr) {
      winrt_input_stream_.Close();
    }
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

BluetoothSocket::BluetoothOutputStream::BluetoothOutputStream(
    IOutputStream stream) {
  winrt_output_stream_ = stream;
}

Exception BluetoothSocket::BluetoothOutputStream::Write(const ByteArray& data) {
  try {
    if (data.size() > kMaxTransmitPacketSize) {
      NEARBY_LOGS(ERROR) << __func__ << ": Transmit packet size " << data.size()
                         << " is too big.";
      return {Exception::kIo};
    }

    std::memcpy(write_buffer_.data(), data.data(), data.size());
    write_buffer_.Length(data.size());

    winrt::hresult hresult =
        winrt_output_stream_.WriteAsync(write_buffer_).get();
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

Exception BluetoothSocket::BluetoothOutputStream::Flush() {
  try {
    if (winrt_output_stream_ == nullptr) {
      return {Exception::kSuccess};
    }

    winrt_output_stream_.FlushAsync().get();
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

Exception BluetoothSocket::BluetoothOutputStream::Close() {
  NEARBY_LOGS(INFO) << __func__ << ": Close bluetooth output stream.";
  try {
    if (winrt_output_stream_ != nullptr) {
      winrt_output_stream_.Close();
    }
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

winrt::fire_and_forget BluetoothSocket::Listener_ConnectionStatusChanged(
    winrt::Windows::Devices::Bluetooth::BluetoothDevice device,
    winrt::Windows::Foundation::IInspectable const& args) {
  // During bandwidth upgrade, Bluetooth connection may be dropped due to
  // unknown reasons. To track this issue, add a log to track the issue.
  // Based on test, the args is empty, so cannot provide more information on
  // the status change.
  BluetoothConnectionStatus connection_status = device.ConnectionStatus();
  NEARBY_LOGS(WARNING) << __func__
                       << ": Bluetooth connection status changed to:"
                       << ((connection_status ==
                            BluetoothConnectionStatus::Connected)
                               ? "Connected"
                               : "Disconnected");
  return {};
}

}  // namespace windows
}  // namespace nearby
