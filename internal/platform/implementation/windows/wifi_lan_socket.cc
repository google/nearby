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
#include "internal/platform/implementation/windows/wifi_lan.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace windows {

WifiLanSocket::WifiLanSocket() {
  enable_blocking_socket_ = NearbyFlags::GetInstance().GetBoolFlag(
      nearby::platform::config_package_nearby::nearby_platform_feature::
          kEnableBlockingSocket);
  client_socket_ = std::make_unique<NearbyClientSocket>();
  input_stream_ = SocketInputStream(client_socket_.get());
  output_stream_ = SocketOutputStream(client_socket_.get());
}

WifiLanSocket::WifiLanSocket(StreamSocket socket) {
  enable_blocking_socket_ = NearbyFlags::GetInstance().GetBoolFlag(
      nearby::platform::config_package_nearby::nearby_platform_feature::
          kEnableBlockingSocket);
  stream_soket_ = socket;
  VLOG(1) << "Socket send buffer size: "
          << socket.Control().OutboundBufferSizeInBytes();
  socket.Control().OutboundBufferSizeInBytes(4 * 1024 * 1024);
  VLOG(1) << "Updated send buffer size to: "
          << socket.Control().OutboundBufferSizeInBytes();
  input_stream_ = SocketInputStream(socket.InputStream());
  output_stream_ = SocketOutputStream(socket.OutputStream());
}

WifiLanSocket::WifiLanSocket(std::unique_ptr<NearbyClientSocket> socket) {
  enable_blocking_socket_ = NearbyFlags::GetInstance().GetBoolFlag(
      nearby::platform::config_package_nearby::nearby_platform_feature::
          kEnableBlockingSocket);
  client_socket_ = std::move(socket);
  input_stream_ = SocketInputStream(client_socket_.get());
  output_stream_ = SocketOutputStream(client_socket_.get());
}

WifiLanSocket::~WifiLanSocket() { Close(); }

InputStream& WifiLanSocket::GetInputStream() { return input_stream_; }

OutputStream& WifiLanSocket::GetOutputStream() { return output_stream_; }

Exception WifiLanSocket::Close() {
  if (enable_blocking_socket_) {
    if (client_socket_ != nullptr) {
      return client_socket_->Close();
    }
  } else {
    try {
      if (stream_soket_ != nullptr) {
        stream_soket_.Close();
      }
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

  return {Exception::kSuccess};
}

bool WifiLanSocket::Connect(const std::string& ip_address, int port) {
  return client_socket_->Connect(ip_address, port);
}

// SocketInputStream
WifiLanSocket::SocketInputStream::SocketInputStream(IInputStream input_stream) {
  enable_blocking_socket_ = NearbyFlags::GetInstance().GetBoolFlag(
      nearby::platform::config_package_nearby::nearby_platform_feature::
          kEnableBlockingSocket);
  input_stream_ = input_stream;
}

WifiLanSocket::SocketInputStream::SocketInputStream(
    NearbyClientSocket* client_socket) {
  enable_blocking_socket_ = NearbyFlags::GetInstance().GetBoolFlag(
      nearby::platform::config_package_nearby::nearby_platform_feature::
          kEnableBlockingSocket);
  client_socket_ = client_socket;
}

ExceptionOr<ByteArray> WifiLanSocket::SocketInputStream::Read(
    std::int64_t size) {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      LOG(ERROR) << "Failed to read data due to no client socket.";
      return {Exception::kIo};
    }

    return client_socket_->Read(size);
  } else {
    try {
      if (read_buffer_ == nullptr || read_buffer_.Capacity() < size) {
        read_buffer_ = Buffer(size);
      }

      // Reset the buffer length to 0.
      read_buffer_.Length(0);

      auto ibuffer =
          input_stream_.ReadAsync(read_buffer_, size, InputStreamOptions::None)
              .get();

      if (ibuffer.Length() != size) {
        LOG(WARNING) << "Only read partial of data: [" << ibuffer.Length()
                     << "/" << size << "].";
      }

      ByteArray data((char*)ibuffer.data(), ibuffer.Length());
      return ExceptionOr(std::move(data));
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

ExceptionOr<size_t> WifiLanSocket::SocketInputStream::Skip(size_t offset) {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      return {Exception::kIo};
    }

    return client_socket_->Skip(offset);
  } else {
    try {
      Buffer buffer = Buffer(offset);

      auto ibuffer =
          input_stream_.ReadAsync(buffer, offset, InputStreamOptions::None)
              .get();
      return ExceptionOr((size_t)ibuffer.Length());
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

Exception WifiLanSocket::SocketInputStream::Close() {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      return {Exception::kIo};
    }

    return client_socket_->Close();
  } else {
    try {
      input_stream_.Close();
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
WifiLanSocket::SocketOutputStream::SocketOutputStream(
    IOutputStream output_stream) {
  enable_blocking_socket_ = NearbyFlags::GetInstance().GetBoolFlag(
      nearby::platform::config_package_nearby::nearby_platform_feature::
          kEnableBlockingSocket);
  output_stream_ = output_stream;
}

WifiLanSocket::SocketOutputStream::SocketOutputStream(
    NearbyClientSocket* client_socket) {
  enable_blocking_socket_ = NearbyFlags::GetInstance().GetBoolFlag(
      nearby::platform::config_package_nearby::nearby_platform_feature::
          kEnableBlockingSocket);
  client_socket_ = client_socket;
}

Exception WifiLanSocket::SocketOutputStream::Write(const ByteArray& data) {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      return {Exception::kIo};
    }

    return client_socket_->Write(data);
  } else {
    try {
      Buffer buffer = Buffer(data.size());
      std::memcpy(buffer.data(), data.data(), data.size());
      buffer.Length(data.size());
      uint32_t wrote_bytes = output_stream_.WriteAsync(buffer).get();
      if (wrote_bytes != data.size()) {
        LOG(WARNING) << "Only wrote partial of data:[" << wrote_bytes << "/"
                     << data.size() << "].";
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

Exception WifiLanSocket::SocketOutputStream::Flush() {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      return {Exception::kIo};
    }

    return client_socket_->Flush();
  } else {
    try {
      output_stream_.FlushAsync().get();
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

Exception WifiLanSocket::SocketOutputStream::Close() {
  if (enable_blocking_socket_) {
    if (client_socket_ == nullptr) {
      return {Exception::kIo};
    }

    return client_socket_->Close();
  } else {
    try {
      output_stream_.Close();
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
