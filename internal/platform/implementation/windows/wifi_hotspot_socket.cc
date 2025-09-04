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
  client_socket_ = std::make_unique<NearbyClientSocket>();
  input_stream_ = SocketInputStream(client_socket_.get());
  output_stream_ = SocketOutputStream(client_socket_.get());
}

WifiHotspotSocket::WifiHotspotSocket(
    std::unique_ptr<NearbyClientSocket> socket) {
  client_socket_ = std::move(socket);
  input_stream_ = SocketInputStream(client_socket_.get());
  output_stream_ = SocketOutputStream(client_socket_.get());
}

WifiHotspotSocket::~WifiHotspotSocket() { Close(); }

InputStream& WifiHotspotSocket::GetInputStream() { return input_stream_; }

OutputStream& WifiHotspotSocket::GetOutputStream() { return output_stream_; }

Exception WifiHotspotSocket::Close() {
  if (client_socket_ != nullptr) {
    return client_socket_->Close();
  }

  return {Exception::kSuccess};
}

bool WifiHotspotSocket::Connect(const std::string& ip_address, int port) {
  return client_socket_->Connect(ip_address, port);
}

WifiHotspotSocket::SocketInputStream::SocketInputStream(
    NearbyClientSocket* client_socket) {
  client_socket_ = client_socket;
}

ExceptionOr<ByteArray> WifiHotspotSocket::SocketInputStream::Read(
    std::int64_t size) {
  if (client_socket_ == nullptr) {
    LOG(ERROR) << "Failed to read data due to no client socket.";
    return {Exception::kIo};
  }

  return client_socket_->Read(size);
}

ExceptionOr<size_t> WifiHotspotSocket::SocketInputStream::Skip(size_t offset) {
  if (client_socket_ == nullptr) {
    return {Exception::kIo};
  }

  return client_socket_->Skip(offset);
}

Exception WifiHotspotSocket::SocketInputStream::Close() {
  if (client_socket_ == nullptr) {
    return {Exception::kIo};
  }

  return client_socket_->Close();
}

// SocketOutputStream
WifiHotspotSocket::SocketOutputStream::SocketOutputStream(
    NearbyClientSocket* client_socket) {
  client_socket_ = client_socket;
}

Exception WifiHotspotSocket::SocketOutputStream::Write(const ByteArray& data) {
  if (client_socket_ == nullptr) {
    return {Exception::kIo};
  }

  return client_socket_->Write(data);
}

Exception WifiHotspotSocket::SocketOutputStream::Flush() {
  if (client_socket_ == nullptr) {
    return {Exception::kIo};
  }

  return client_socket_->Flush();
}

Exception WifiHotspotSocket::SocketOutputStream::Close() {
  if (client_socket_ == nullptr) {
    return {Exception::kIo};
  }

  return client_socket_->Close();
}

}  // namespace windows
}  // namespace nearby
