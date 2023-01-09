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

#include "internal/platform/implementation/windows/ble_v2_socket.h"

#include <exception>
#include <utility>

#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {

BleV2Socket::BleV2Socket(StreamSocket streamSocket)
    : windows_socket_(streamSocket) {}

BleV2Socket::BleV2Socket() {}

BleV2Socket::~BleV2Socket() {}

// NOTE:
// It is an undefined behavior if GetInputStream() or GetOutputStream() is
// called for a not-connected BluetoothSocket, i.e. any object that is not
// returned by BluetoothClassicMedium::ConnectToService() for client side or
// BluetoothServerSocket::Accept() for server side of connection.
// Returns the InputStream of this connected BluetoothSocket.
InputStream& BleV2Socket::GetInputStream() { return input_stream_; }

// Returns the OutputStream of this connected BluetoothSocket.
OutputStream& BleV2Socket::GetOutputStream() { return output_stream_; }

// Closes both input and output streams, marks Socket as closed.
// After this call object should be treated as not connected.
// Returns Exception::kIo on error, Exception::kSuccess otherwise.
Exception BleV2Socket::Close() {
  // The Close method aborts any pending operations and releases all unmanaged
  // resources associated with the StreamSocket object, including the Input and
  // Output streams
  try {
    if (windows_socket_ != nullptr) {
      windows_socket_.Close();
      windows_socket_ = nullptr;
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

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
// Returns valid BluetoothDevice pointer if there is a connection, and
// nullptr otherwise.
api::ble_v2::BlePeripheral* BleV2Socket::GetRemotePeripheral() {
  return bluetooth_device_.get();
}

// Starts an asynchronous operation on a StreamSocket object to connect to a
// remote network destination specified by a remote hostname and a remote
// service name.
bool BleV2Socket::Connect(HostName connectionHostName,
                          winrt::hstring connectionServiceName) {
  return false;
}

BleV2Socket::BluetoothInputStream::BluetoothInputStream(IInputStream stream) {
  winrt_stream_ = stream;
}

ExceptionOr<ByteArray> BleV2Socket::BluetoothInputStream::Read(
    std::int64_t size) {
  try {
    if (winrt_stream_ == nullptr) {
      return {Exception::kIo};
    }

    Buffer buffer = Buffer(size);

    auto ibuffer =
        winrt_stream_.ReadAsync(buffer, size, InputStreamOptions::None).get();

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

IAsyncAction BleV2Socket::CancelIOAsync() {
  // Cancels pending reads and writes over a StreamSocket object.
  // https://docs.microsoft.com/en-us/uwp/api/windows.networking.sockets.streamsocket.cancelioasync?view=winrt-20348
  return windows_socket_.CancelIOAsync();
}

Exception BleV2Socket::BluetoothInputStream::Close() {
  try {
    if (winrt_stream_ == nullptr) {
      return {Exception::kSuccess};  // Already closed, don't error out
    }

    winrt_stream_.Close();
    winrt_stream_ = nullptr;
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

BleV2Socket::BluetoothOutputStream::BluetoothOutputStream(
    IOutputStream stream) {
  winrt_stream_ = stream;
}

Exception BleV2Socket::BluetoothOutputStream::Write(const ByteArray& data) {
  try {
    if (winrt_stream_ == nullptr) {
      return {Exception::kIo};
    }

    Buffer buffer = Buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());
    buffer.Length(data.size());

    winrt::hresult hresult = winrt_stream_.WriteAsync(buffer).get();
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

Exception BleV2Socket::BluetoothOutputStream::Flush() {
  try {
    if (winrt_stream_ == nullptr) {
      return {Exception::kSuccess};
    }

    winrt_stream_.FlushAsync().get();
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

Exception BleV2Socket::BluetoothOutputStream::Close() {
  try {
    if (winrt_stream_ == nullptr) {
      return {Exception::kSuccess};  // Already closed, don't error out
    }

    winrt_stream_.Close();
    winrt_stream_ = nullptr;
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

}  // namespace windows
}  // namespace nearby
