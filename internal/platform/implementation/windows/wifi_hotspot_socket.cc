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
#include <memory>
#include <string>
#include <utility>

#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/wifi_hotspot.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace windows {
namespace {

int recv_sync(SOCKET s, char* buf, int len, int flags) {
  int result;
  struct fd_set read_fds;

  result = recv(s, buf, len, flags);
  if (result >= 0) {
    return result;
  }

  while (WSAGetLastError() == WSAEWOULDBLOCK) {
    FD_ZERO(&read_fds);
    FD_SET(s, &read_fds);
    if ((select(/*nfds=*/0, /*readfds=*/&read_fds, /*writefds=*/nullptr,
                /*exceptfds=*/nullptr, /*timeout=*/nullptr) > 0) &&
        FD_ISSET(s, &read_fds)) {
      result = recv(s, buf, len, flags);
      if (result >= 0) {
        return result;
      }
    }
  }

  return -1;
}

int send_sync(SOCKET s, const char* buf, int len, int flags) {
  int result;
  struct fd_set write_fds;

  result = send(s, buf, len, flags);
  if (result >= 0) {
    return result;
  }

  while (WSAGetLastError() == WSAEWOULDBLOCK) {
    FD_ZERO(&write_fds);
    FD_SET(s, &write_fds);
    if ((select(/*nfds=*/0, /*readfds=*/nullptr, /*writefds=*/&write_fds,
                /*exceptfds=*/nullptr, /*timeout=*/nullptr) > 0) &&
        FD_ISSET(s, &write_fds)) {
      result = send(s, buf, len, flags);
      if (result >= 0) {
        return result;
      }
    }
  }

  return -1;
}

}  // namespace

WifiHotspotSocket::WifiHotspotSocket() {
  enable_blocking_socket_ = NearbyFlags::GetInstance().GetBoolFlag(
      nearby::platform::config_package_nearby::nearby_platform_feature::
          kEnableBlockingSocket);
  client_socket_ = std::make_unique<NearbyClientSocket>();
  input_stream_ = SocketInputStream(client_socket_.get());
  output_stream_ = SocketOutputStream(client_socket_.get());
}

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

WifiHotspotSocket::WifiHotspotSocket(
    std::unique_ptr<NearbyClientSocket> socket) {
  enable_blocking_socket_ = NearbyFlags::GetInstance().GetBoolFlag(
      nearby::platform::config_package_nearby::nearby_platform_feature::
          kEnableBlockingSocket);
  client_socket_ = std::move(socket);
  input_stream_ = SocketInputStream(client_socket_.get());
  output_stream_ = SocketOutputStream(client_socket_.get());
}

WifiHotspotSocket::~WifiHotspotSocket() { Close(); }

InputStream& WifiHotspotSocket::GetInputStream() { return input_stream_; }

OutputStream& WifiHotspotSocket::GetOutputStream() { return output_stream_; }

Exception WifiHotspotSocket::Close() {
  if (enable_blocking_socket_) {
    if (client_socket_ != nullptr) {
      return client_socket_->Close();
    }

    return {Exception::kSuccess};
  } else {
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
      LOG(ERROR) << __func__ << ": Unknown exception.";
      return {Exception::kIo};
    }
  }
}

bool WifiHotspotSocket::Connect(const std::string& ip_address, int port) {
  return client_socket_->Connect(ip_address, port);
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

WifiHotspotSocket::SocketInputStream::SocketInputStream(
    NearbyClientSocket* client_socket) {
  enable_blocking_socket_ = NearbyFlags::GetInstance().GetBoolFlag(
      nearby::platform::config_package_nearby::nearby_platform_feature::
          kEnableBlockingSocket);
  client_socket_ = client_socket;
}

ExceptionOr<ByteArray> WifiHotspotSocket::SocketInputStream::Read(
    std::int64_t size) {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      LOG(ERROR) << "Failed to read data due to no client socket.";
      return {Exception::kIo};
    }

    return client_socket_->Read(size);
  } else {
    try {
      if (socket_type_ == SocketType::kWinRTSocket) {
        Buffer buffer = Buffer(size);

        auto ibuffer =
            input_stream_.ReadAsync(buffer, size, InputStreamOptions::None)
                .get();

        if (ibuffer.Length() != size) {
          LOG(WARNING) << "Only got part of data of needed.";
        }

        ByteArray data((char*)ibuffer.data(), ibuffer.Length());
        return ExceptionOr<ByteArray>(data);
      }

      int result;
      int count = 0;

      // When socket_type_ == SocketType::kWin32Socket
      if (size > read_buffer_.size()) {
        read_buffer_.resize(size);
      }

      while (count < size) {
        result =
            recv_sync(socket_, read_buffer_.data() + count, size - count, 0);
        if (result == 0) {
          LOG(WARNING) << "Connection closed.";
          return {Exception::kIo};
        } else if (result < 0) {
          LOG(ERROR) << "recv failed with error: " << WSAGetLastError();
          return {Exception::kIo};
        } else {
          count += result;
        }
      }

      ByteArray data(read_buffer_.data(), size);
      return ExceptionOr<ByteArray>(data);
    } catch (std::exception exception) {
      LOG(ERROR) << __func__ << ": Exception: " << exception.what();
      return {Exception::kIo};
    } catch (const winrt::hresult_error& error) {
      LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
                 << winrt::to_string(error.message());
      return {Exception::kIo};
    } catch (...) {
      LOG(ERROR) << __func__ << ": Unknown exception.";
      return {Exception::kIo};
    }
  }
}

ExceptionOr<size_t> WifiHotspotSocket::SocketInputStream::Skip(size_t offset) {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      return {Exception::kIo};
    }

    return client_socket_->Skip(offset);
  } else {
    try {
      if (socket_type_ == SocketType::kWinRTSocket) {
        Buffer buffer = Buffer(offset);

        auto ibuffer =
            input_stream_.ReadAsync(buffer, offset, InputStreamOptions::None)
                .get();
        return ExceptionOr<size_t>((size_t)ibuffer.Length());
      }
      // When socket_type_ == SocketType::kWin32Socket
      int result;
      int count = 0;

      // When socket_type_ == SocketType::kWin32Socket
      if (offset > read_buffer_.size()) {
        read_buffer_.resize(offset);
      }

      while (count < offset) {
        result =
            recv_sync(socket_, read_buffer_.data() + count, offset - count, 0);
        if (result == 0) {
          LOG(WARNING) << "Connection closed.";
          return {Exception::kIo};
        } else if (result < 0) {
          LOG(ERROR) << "recv failed with error: " << WSAGetLastError();
          return {Exception::kIo};
        } else {
          count += result;
        }
      }

      return ExceptionOr<size_t>(offset);
    } catch (std::exception exception) {
      LOG(ERROR) << __func__ << ": Exception: " << exception.what();
      return {Exception::kIo};
    } catch (const winrt::hresult_error& error) {
      LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
                 << winrt::to_string(error.message());
      return {Exception::kIo};
    } catch (...) {
      LOG(ERROR) << __func__ << ": Unknown exception.";
      return {Exception::kIo};
    }
  }
}

Exception WifiHotspotSocket::SocketInputStream::Close() {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      return {Exception::kIo};
    }

    return client_socket_->Close();
  } else {
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
      LOG(ERROR) << __func__ << ": Unknown exception.";
      return {Exception::kIo};
    }
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

WifiHotspotSocket::SocketOutputStream::SocketOutputStream(
    NearbyClientSocket* client_socket) {
  enable_blocking_socket_ = NearbyFlags::GetInstance().GetBoolFlag(
      nearby::platform::config_package_nearby::nearby_platform_feature::
          kEnableBlockingSocket);
  client_socket_ = client_socket;
}

Exception WifiHotspotSocket::SocketOutputStream::Write(const ByteArray& data) {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      return {Exception::kIo};
    }

    return client_socket_->Write(data);
  } else {
    try {
      if (socket_type_ == SocketType::kWinRTSocket) {
        Buffer buffer = Buffer(data.size());
        std::memcpy(buffer.data(), data.data(), data.size());
        buffer.Length(data.size());

        output_stream_.WriteAsync(buffer).get();
        return {Exception::kSuccess};
      }

      int result;
      int count = 0;

      while (count < data.size()) {
        result =
            send_sync(socket_, data.data() + count, data.size() - count, 0);
        if (result == 0) {
          LOG(WARNING) << "Connection closed.";
          return {Exception::kIo};
        } else if (result < 0) {
          LOG(ERROR) << "Failed to send data with error: " << WSAGetLastError();
          return {Exception::kIo};
        } else {
          count += result;
        }
      }

      return {Exception::kSuccess};
    }

    catch (std::exception exception) {
      LOG(ERROR) << __func__ << ": Exception: " << exception.what();
      return {Exception::kIo};
    } catch (const winrt::hresult_error& error) {
      LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
                 << winrt::to_string(error.message());
      return {Exception::kIo};
    } catch (...) {
      LOG(ERROR) << __func__ << ": Unknown exception.";
      return {Exception::kIo};
    }
  }
}

Exception WifiHotspotSocket::SocketOutputStream::Flush() {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      return {Exception::kIo};
    }

    return client_socket_->Flush();
  } else {
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
      LOG(ERROR) << __func__ << ": Unknown exception.";
      return {Exception::kIo};
    }
  }
}

Exception WifiHotspotSocket::SocketOutputStream::Close() {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      return {Exception::kIo};
    }

    return client_socket_->Close();
  } else {
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
      LOG(ERROR) << __func__ << ": Unknown exception.";
      return {Exception::kIo};
    }
  }
}

}  // namespace windows
}  // namespace nearby
