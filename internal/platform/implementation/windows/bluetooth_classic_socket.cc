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

#include <exception>
#include <utility>

#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/logging.h"

namespace location {
namespace nearby {
namespace windows {

BluetoothSocket::BluetoothSocket(StreamSocket streamSocket)
    : windows_socket_(streamSocket) {
  bluetooth_device_ = std::make_unique<BluetoothDevice>(
      winrt::Windows::Devices::Bluetooth::BluetoothDevice::FromHostNameAsync(
          windows_socket_.Information().RemoteHostName())
          .get());
  input_stream_ =
      std::make_unique<BluetoothInputStream>(windows_socket_.InputStream());
  output_stream_ =
      std::make_unique<BluetoothOutputStream>(windows_socket_.OutputStream());
}

BluetoothSocket::BluetoothSocket() {}

BluetoothSocket::~BluetoothSocket() {}

// NOTE:
// It is an undefined behavior if GetInputStream() or GetOutputStream() is
// called for a not-connected BluetoothSocket, i.e. any object that is not
// returned by BluetoothClassicMedium::ConnectToService() for client side or
// BluetoothServerSocket::Accept() for server side of connection.
// Returns the InputStream of this connected BluetoothSocket.
InputStream& BluetoothSocket::GetInputStream() { return *input_stream_.get(); }

// Returns the OutputStream of this connected BluetoothSocket.
OutputStream& BluetoothSocket::GetOutputStream() {
  return *output_stream_.get();
}

// Closes both input and output streams, marks Socket as closed.
// After this call object should be treated as not connected.
// Returns Exception::kIo on error, Exception::kSuccess otherwise.
Exception BluetoothSocket::Close() {
  // The Close method aborts any pending operations and releases all unmanaged
  // resources associated with the StreamSocket object, including the Input and
  // Output streams
  try {
    if (windows_socket_ != nullptr) {
      windows_socket_.Close();
      windows_socket_ = nullptr;
    }
    return {Exception::kSuccess};
  } catch (...) {
    NEARBY_LOGS(ERROR) << "Failed to close Bluetooth socket.";
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
  if (connectionHostName == nullptr || connectionServiceName.empty()) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Bluetooth socket connection failed. Attempting to "
           "connect to empty HostName/MAC address or ServiceName.";
    return false;
  }

  windows_socket_ = winrt::Windows::Networking::Sockets::StreamSocket();

  // https://docs.microsoft.com/en-us/uwp/api/windows.networking.sockets.streamsocket.connectasync?view=winrt-20348
  windows_socket_.ConnectAsync(connectionHostName, connectionServiceName).get();

  auto info = windows_socket_.Information();
  auto hostName = info.RemoteHostName();

  bluetooth_device_ = std::make_unique<BluetoothDevice>(
      winrt::Windows::Devices::Bluetooth::BluetoothDevice::FromHostNameAsync(
          windows_socket_.Information().RemoteHostName())
          .get());

  input_stream_ =
      std::make_unique<BluetoothInputStream>(windows_socket_.InputStream());
  output_stream_ =
      std::make_unique<BluetoothOutputStream>(windows_socket_.OutputStream());

  NEARBY_LOGS(INFO) << __func__
                    << ": Bluetooth socket successfully connected to "
                    << bluetooth_device_->GetName();
  return true;
}

BluetoothSocket::BluetoothInputStream::BluetoothInputStream(
    IInputStream stream) {
  winrt_stream_ = stream;
}

ExceptionOr<ByteArray> BluetoothSocket::BluetoothInputStream::Read(
    std::int64_t size) {
  try {
    if (winrt_stream_ == nullptr) {
      return {Exception::kIo};
    }

    Buffer buffer = Buffer(size);
    winrt_stream_.ReadAsync(buffer, size, InputStreamOptions::Partial).get();
    DataReader dataReader = DataReader::FromBuffer(buffer);
    ByteArray data((char*)buffer.data(), buffer.Length());
    return ExceptionOr(data);
  } catch (...) {
    NEARBY_LOGS(ERROR) << "Failed to read data from input stream.";
    return {Exception::kIo};
  }
}

IAsyncAction BluetoothSocket::CancelIOAsync() {
  // Cancels pending reads and writes over a StreamSocket object.
  // https://docs.microsoft.com/en-us/uwp/api/windows.networking.sockets.streamsocket.cancelioasync?view=winrt-20348
  return windows_socket_.CancelIOAsync();
}

Exception BluetoothSocket::BluetoothInputStream::Close() {
  try {
    if (winrt_stream_ == nullptr) {
      return {Exception::kSuccess};  // Already closed, don't error out
    }

    winrt_stream_.Close();
    winrt_stream_ = nullptr;
    return {Exception::kSuccess};
  } catch (...) {
    NEARBY_LOGS(ERROR) << "Failed to close input stream.";
    return {Exception::kIo};
  }
}

BluetoothSocket::BluetoothOutputStream::BluetoothOutputStream(
    IOutputStream stream) {
  winrt_stream_ = stream;
}

Exception BluetoothSocket::BluetoothOutputStream::Write(const ByteArray& data) {
  try {
    if (winrt_stream_ == nullptr) {
      return {Exception::kIo};
    }

    Buffer buffer = Buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());
    buffer.Length(data.size());

    winrt::hresult hresult = winrt_stream_.WriteAsync(buffer).get();
    return {Exception::kSuccess};
  } catch (winrt::hresult_error const& ex) {
    NEARBY_LOGS(ERROR) << __func__ << ": winrt exception: " << ex.code() << ": "
                       << winrt::to_string(ex.message());

    return {Exception::kIo};
  }
}

Exception BluetoothSocket::BluetoothOutputStream::Flush() {
  try {
    if (winrt_stream_ == nullptr) {
      return {Exception::kSuccess};
    }

    winrt_stream_.FlushAsync().get();
    return {Exception::kSuccess};
  } catch (...) {
    NEARBY_LOGS(ERROR) << "Failed to flush data.";
    return {Exception::kIo};
  }
}

Exception BluetoothSocket::BluetoothOutputStream::Close() {
  try {
    if (winrt_stream_ == nullptr) {
      return {Exception::kSuccess};  // Already closed, don't error out
    }

    winrt_stream_.Close();
    winrt_stream_ = nullptr;
    return {Exception::kSuccess};
  } catch (...) {
    NEARBY_LOGS(ERROR) << "Failed to close output stream.";
    return {Exception::kIo};
  }
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
