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
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Storage.Streams.h"
#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/wifi_lan.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/output_stream.h"

namespace nearby::windows {
namespace {
using ::winrt::Windows::Networking::Sockets::StreamSocket;
using ::winrt::Windows::Storage::Streams::IInputStream;
using ::winrt::Windows::Storage::Streams::IOutputStream;
}  // namespace

WifiLanSocket::WifiLanSocket() {
  client_socket_ = std::make_unique<NearbyClientSocket>();
  input_stream_ = SocketInputStream(client_socket_.get());
  output_stream_ = SocketOutputStream(client_socket_.get());
}

WifiLanSocket::WifiLanSocket(StreamSocket socket) {
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
  client_socket_ = std::move(socket);
  input_stream_ = SocketInputStream(client_socket_.get());
  output_stream_ = SocketOutputStream(client_socket_.get());
}

WifiLanSocket::~WifiLanSocket() { Close(); }

InputStream& WifiLanSocket::GetInputStream() { return input_stream_; }

OutputStream& WifiLanSocket::GetOutputStream() { return output_stream_; }

Exception WifiLanSocket::Close() {
  if (client_socket_ != nullptr) {
    return client_socket_->Close();
  }
  return {Exception::kSuccess};
}

bool WifiLanSocket::Connect(const std::string& ip_address,
                            int port, bool dual_stack) {
  return client_socket_->Connect(ip_address, port, dual_stack);
}

// SocketInputStream
WifiLanSocket::SocketInputStream::SocketInputStream(IInputStream input_stream) {
  input_stream_ = input_stream;
}

WifiLanSocket::SocketInputStream::SocketInputStream(
    NearbyClientSocket* client_socket) {
  client_socket_ = client_socket;
}

ExceptionOr<ByteArray> WifiLanSocket::SocketInputStream::Read(
    std::int64_t size) {
  if (client_socket_ == nullptr) {
    LOG(ERROR) << "Failed to read data due to no client socket.";
    return {Exception::kIo};
  }

  return client_socket_->Read(size);
}

ExceptionOr<size_t> WifiLanSocket::SocketInputStream::Skip(size_t offset) {
  if (client_socket_ == nullptr) {
    return {Exception::kIo};
  }

  return client_socket_->Skip(offset);
}

Exception WifiLanSocket::SocketInputStream::Close() {
  if (client_socket_ == nullptr) {
    return {Exception::kIo};
  }

  return client_socket_->Close();
}

// SocketOutputStream
WifiLanSocket::SocketOutputStream::SocketOutputStream(
    IOutputStream output_stream) {
  output_stream_ = output_stream;
}

WifiLanSocket::SocketOutputStream::SocketOutputStream(
    NearbyClientSocket* client_socket) {
  client_socket_ = client_socket;
}

Exception WifiLanSocket::SocketOutputStream::Write(const ByteArray& data) {
  if (client_socket_ == nullptr) {
    return {Exception::kIo};
  }

  return client_socket_->Write(data);
}

Exception WifiLanSocket::SocketOutputStream::Flush() {
  if (client_socket_ == nullptr) {
    return {Exception::kIo};
  }

  return client_socket_->Flush();
}

Exception WifiLanSocket::SocketOutputStream::Close() {
  if (client_socket_ == nullptr) {
    return {Exception::kIo};
  }

  return client_socket_->Close();
}

}  // namespace nearby::windows
