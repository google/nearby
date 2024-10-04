// Copyright 2021 Google LLC
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

#include <cstdint>
#include <cstring>
#include <exception>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/wifi_hotspot.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace windows {
namespace {
// Recorded the maximum bytes received in one read is 65586 in the test, but add
// a little more for const kMaxByteRecieved for safty reason.
constexpr int kMaxByteRecieved = 66000;
}  // namespace

WifiHotspotSocket::WifiHotspotSocket(StreamSocket socket) {
  stream_soket_ = socket;
  input_stream_ = SocketInputStream(socket.InputStream());
  output_stream_ = SocketOutputStream(socket.OutputStream());
}

WifiHotspotSocket::WifiHotspotSocket(SOCKET socket) {
  stream_soket_winsock_ = socket;
  input_stream_ = SocketInputStream(socket);
  output_stream_ = SocketOutputStream(socket);
}

WifiHotspotSocket::~WifiHotspotSocket() {
  try {
    if (stream_soket_ != nullptr || stream_soket_winsock_ != INVALID_SOCKET) {
      Close();
    }
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }
}

InputStream& WifiHotspotSocket::GetInputStream() { return input_stream_; }

OutputStream& WifiHotspotSocket::GetOutputStream() { return output_stream_; }

Exception WifiHotspotSocket::Close() {
  try {
    if (stream_soket_ != nullptr) {
      stream_soket_.Close();
    }
    if (stream_soket_winsock_ != INVALID_SOCKET) {
      closesocket(stream_soket_winsock_);
      stream_soket_winsock_ = INVALID_SOCKET;
    }
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

WifiHotspotSocket::SocketInputStream::SocketInputStream(
    IInputStream input_stream) {
  input_stream_ = input_stream;
  socket_type_ = SocketType::kWinRTSocket;
}

WifiHotspotSocket::SocketInputStream::SocketInputStream(SOCKET socket) {
  socket_ = socket;
  socket_type_ = SocketType::kWin32Socket;
}

ExceptionOr<ByteArray> WifiHotspotSocket::SocketInputStream::Read(
    std::int64_t size) {
  try {
    if (socket_type_ == SocketType::kWinRTSocket) {
      Buffer buffer = Buffer(size);

      auto ibuffer =
          input_stream_.ReadAsync(buffer, size, InputStreamOptions::None).get();

      if (ibuffer.Length() != size) {
        LOG(WARNING) << "Only got part of data of needed.";
      }

      ByteArray data((char*)ibuffer.data(), ibuffer.Length());
      return ExceptionOr<ByteArray>(data);
    }
    // When socket_type_ == SocketType::kWin32Socket
    char recv_buf[kMaxByteRecieved];
    int result;
    struct fd_set readfds;

    result = recv(socket_, recv_buf, size, 0);
    if (result > 0) {
      ByteArray data(recv_buf, result);
      return ExceptionOr<ByteArray>(data);
    }
    if (result == 0) {
      LOG(INFO) << "Connection closed.";
      return {Exception::kIo};
    }
    // When WSAEWOULDBLOCK happens, it means the packet for receive is not
    // ready at the moment. The API select() will block till the packet is
    // ready for recieving.
    if (WSAEWOULDBLOCK == WSAGetLastError()) {
      FD_ZERO(&readfds);
      FD_SET(socket_, &readfds);
      if ((select(/*nfds ignored*/ 0, &readfds, nullptr, nullptr, nullptr) >
           0) &&
          FD_ISSET(socket_, &readfds)) {
        result = recv(socket_, recv_buf, size, 0);
        if (result > 0) {
          ByteArray data(recv_buf, result);
          return ExceptionOr<ByteArray>(data);
        }
      }
    }
    return {Exception::kIo};
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

ExceptionOr<size_t> WifiHotspotSocket::SocketInputStream::Skip(size_t offset) {
  try {
    if (socket_type_ == SocketType::kWinRTSocket) {
      Buffer buffer = Buffer(offset);

      auto ibuffer =
          input_stream_.ReadAsync(buffer, offset, InputStreamOptions::None)
              .get();
      return ExceptionOr<size_t>((size_t)ibuffer.Length());
    }
    // When socket_type_ == SocketType::kWin32Socket
    char recv_buf[kMaxByteRecieved];
    int result;
    struct fd_set readfds;

    result = recv(socket_, recv_buf, offset, 0);
    if (result > 0) {
      return ExceptionOr<size_t>((size_t)result);
    }
    if (result == 0) {
      LOG(INFO) << "Connection closed.";
    } else {
      // When WSAEWOULDBLOCK happens, it means the packet for receive is not
      // ready at the moment. The API select() will block till the packet is
      // ready for recieving.
      if (WSAEWOULDBLOCK == WSAGetLastError()) {
        FD_ZERO(&readfds);
        FD_SET(socket_, &readfds);
        if ((select(/*nfds ignored*/ 0, &readfds, nullptr, nullptr, nullptr) >
             0) &&
            FD_ISSET(socket_, &readfds)) {
          result = recv(socket_, recv_buf, offset, 0);
          if (result > 0) {
            return ExceptionOr<size_t>((size_t)result);
          }
        }
      }
    }
    return {Exception::kIo};
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

Exception WifiHotspotSocket::SocketInputStream::Close() {
  try {
    if (socket_type_ == SocketType::kWinRTSocket) {
      input_stream_.Close();
    } else {
      // When socket_type_ == SocketType::kWin32Socket
      shutdown(socket_, SD_RECEIVE);
    }
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

// SocketOutputStream
WifiHotspotSocket::SocketOutputStream::SocketOutputStream(
    IOutputStream output_stream) {
  output_stream_ = output_stream;
  socket_type_ = SocketType::kWinRTSocket;
}

WifiHotspotSocket::SocketOutputStream::SocketOutputStream(SOCKET socket) {
  socket_ = socket;
  socket_type_ = SocketType::kWin32Socket;
}

Exception WifiHotspotSocket::SocketOutputStream::Write(const ByteArray& data) {
  try {
    if (socket_type_ == SocketType::kWinRTSocket) {
      Buffer buffer = Buffer(data.size());
      std::memcpy(buffer.data(), data.data(), data.size());
      buffer.Length(data.size());

      output_stream_.WriteAsync(buffer).get();
      return {Exception::kSuccess};
    }
    // When socket_type_ == SocketType::kWin32Socket
    char sendbuf[kMaxByteRecieved];
    int result;

    std::memcpy(sendbuf, data.data(), data.size());
    result = send(socket_, sendbuf, data.size(), 0);
    if (result > 0) {
      return {Exception::kSuccess};
    }
    LOG(INFO) << "recv failed: " << WSAGetLastError();

    return {Exception::kIo};
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

Exception WifiHotspotSocket::SocketOutputStream::Flush() {
  try {
    if (socket_type_ == SocketType::kWinRTSocket) {
      output_stream_.FlushAsync().get();
    }
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

Exception WifiHotspotSocket::SocketOutputStream::Close() {
  try {
    if (socket_type_ == SocketType::kWinRTSocket) {
      output_stream_.Close();
    } else {
      // When socket_type_ == SocketType::kWin32Socket
      shutdown(socket_, SD_SEND);
    }
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

}  // namespace windows
}  // namespace nearby
