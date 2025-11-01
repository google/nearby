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
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/wifi_lan.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby::windows {

WifiLanSocket::WifiLanSocket()
    : WifiLanSocket(std::make_unique<NearbyClientSocket>()) {}

WifiLanSocket::WifiLanSocket(
    absl_nonnull std::unique_ptr<NearbyClientSocket> socket)
    : client_socket_(std::move(socket)),
      input_stream_(client_socket_.get()),
      output_stream_(client_socket_.get()) {}

WifiLanSocket::~WifiLanSocket() { Close(); }

InputStream& WifiLanSocket::GetInputStream() { return input_stream_; }

OutputStream& WifiLanSocket::GetOutputStream() { return output_stream_; }

Exception WifiLanSocket::Close() {
  return client_socket_->Close();
}

bool WifiLanSocket::Connect(const SocketAddress& server_address) {
  return client_socket_->Connect(server_address);
}

// SocketInputStream
WifiLanSocket::SocketInputStream::SocketInputStream(
    NearbyClientSocket* absl_nonnull client_socket)
    : client_socket_(client_socket) {}

ExceptionOr<ByteArray> WifiLanSocket::SocketInputStream::Read(
    std::int64_t size) {
  return client_socket_->Read(size);
}

ExceptionOr<size_t> WifiLanSocket::SocketInputStream::Skip(size_t offset) {
  return client_socket_->Skip(offset);
}

Exception WifiLanSocket::SocketInputStream::Close() {
  return client_socket_->Close();
}

// SocketOutputStream
WifiLanSocket::SocketOutputStream::SocketOutputStream(
    NearbyClientSocket* absl_nonnull client_socket)
    : client_socket_(client_socket) {}

Exception WifiLanSocket::SocketOutputStream::Write(const ByteArray& data) {
  return client_socket_->Write(data);
}

Exception WifiLanSocket::SocketOutputStream::Flush() {
  return client_socket_->Flush();
}

Exception WifiLanSocket::SocketOutputStream::Close() {
  return client_socket_->Close();
}

}  // namespace nearby::windows
