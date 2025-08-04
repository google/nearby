// Copyright 2025 Google LLC
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

#import "internal/platform/implementation/apple/awdl.h"
#import <Foundation/Foundation.h>

#include <memory>
#include <string>
#include <utility>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFramework.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"
#import "internal/platform/implementation/apple/network_utils.h"

namespace nearby {
namespace apple {

#pragma mark - AwdlInputStream

AwdlInputStream::AwdlInputStream(GNCNWFrameworkSocket* socket) : socket_(socket) {}

ExceptionOr<ByteArray> AwdlInputStream::Read(std::int64_t size) {
  NSError* error = nil;
  NSData* data = [socket_ readMaxLength:size error:&error];
  if (data == nil) {
    GNCLoggerError(@"Error reading socket: %@", error);
    return {Exception::kIo};
  }
  return ExceptionOr<ByteArray>{ByteArray((const char*)data.bytes, data.length)};
}

Exception AwdlInputStream::Close() {
  // The input stream reads directly from the connection. It can not be closed without closing the
  // connection itself. A call to `AwdlSocket::Close` will close the connection.
  return {Exception::kSuccess};
}

#pragma mark - AwdlOutputStream

AwdlOutputStream::AwdlOutputStream(GNCNWFrameworkSocket* socket) : socket_(socket) {}

Exception AwdlOutputStream::Write(const ByteArray& data) {
  NSError* error = nil;
  BOOL result = [socket_ write:[NSData dataWithBytes:data.data() length:data.size()] error:&error];
  if (!result) {
    GNCLoggerError(@"Error writing socket: %@", error);
    return {Exception::kIo};
  }
  return {Exception::kSuccess};
}

Exception AwdlOutputStream::Flush() {
  // Write blocks until the data has successfully been written/received, so no more work is needed
  // to flush.
  return {Exception::kSuccess};
}

Exception AwdlOutputStream::Close() {
  // The output stream writes directly to the connection. It can not be closed without closing the
  // connection itself. A call to `AwdlSocket::Close` will close the connection.
  return {Exception::kSuccess};
}

#pragma mark - AwdlSocket

AwdlSocket::AwdlSocket(GNCNWFrameworkSocket* socket)
    : socket_(socket),
      input_stream_(std::make_unique<AwdlInputStream>(socket)),
      output_stream_(std::make_unique<AwdlOutputStream>(socket)) {}

InputStream& AwdlSocket::GetInputStream() { return *input_stream_; }

OutputStream& AwdlSocket::GetOutputStream() { return *output_stream_; }

Exception AwdlSocket::Close() {
  [socket_ close];
  return {Exception::kSuccess};
}

#pragma mark - AwdlServerSocket

AwdlServerSocket::AwdlServerSocket(GNCNWFrameworkServerSocket* server_socket)
    : server_socket_(server_socket) {}

std::string AwdlServerSocket::GetIPAddress() const {
  NSData* addressData = server_socket_.ipAddress.binaryRepresentation;
  return std::string((char*)addressData.bytes, addressData.length);
}

int AwdlServerSocket::GetPort() const { return server_socket_.port; }

std::unique_ptr<api::AwdlSocket> AwdlServerSocket::Accept() {
  NSError* error = nil;
  GNCNWFrameworkSocket* socket = [server_socket_ acceptWithError:&error];
  if (socket != nil) {
    return std::make_unique<AwdlSocket>(socket);
  }
  if (error != nil) {
    GNCLoggerError(@"Error accepting socket: %@", error);
  }
  return nil;
}

Exception AwdlServerSocket::Close() {
  [server_socket_ close];
  return {Exception::kSuccess};
}

#pragma mark - AwdlMedium

AwdlMedium::AwdlMedium() { medium_ = [[GNCNWFramework alloc] init]; }

AwdlMedium::AwdlMedium(GNCNWFramework* medium) : medium_(medium) {}

bool AwdlMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  return network_utils::StartAdvertising(medium_, nsd_service_info);
}

bool AwdlMedium::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  return network_utils::StopAdvertising(medium_, nsd_service_info);
}

bool AwdlMedium::StartDiscovery(const std::string& service_type,
                                DiscoveredServiceCallback callback) {
  service_callback_ = std::move(callback);
  network_utils::NetworkDiscoveredServiceCallback network_callback = {
      .network_service_discovered_cb =
          [this](const NsdServiceInfo& service_info) {
            service_callback_.service_discovered_cb(service_info);
          },
      .network_service_lost_cb =
          [this](const NsdServiceInfo& service_info) {
            service_callback_.service_lost_cb(service_info);
          }};
  return network_utils::StartDiscovery(medium_, service_type, std::move(network_callback),
                                       /*include_peer_to_peer=*/true);
}

bool AwdlMedium::StopDiscovery(const std::string& service_type) {
  return network_utils::StopDiscovery(medium_, service_type);
}

std::unique_ptr<api::AwdlSocket> AwdlMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info, CancellationFlag* cancellation_flag) {
  GNCNWFrameworkSocket* socket =
      network_utils::ConnectToService(medium_, remote_service_info, cancellation_flag);
  if (socket != nil) {
    return std::make_unique<AwdlSocket>(socket);
  }
  return nil;
}

std::unique_ptr<api::AwdlSocket> AwdlMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info, const api::PskInfo& psk_info,
    CancellationFlag* cancellation_flag) {
  GNCNWFrameworkSocket* socket =
      network_utils::ConnectToService(medium_, remote_service_info, psk_info, cancellation_flag);
  if (socket != nil) {
    return std::make_unique<AwdlSocket>(socket);
  }
  return nil;
}

std::unique_ptr<api::AwdlServerSocket> AwdlMedium::ListenForService(int port) {
  GNCNWFrameworkServerSocket* serverSocket =
      network_utils::ListenForService(medium_, port, /*include_peer_to_peer=*/true);
  if (serverSocket != nil) {
    return std::make_unique<AwdlServerSocket>(serverSocket);
  }
  return nil;
}

std::unique_ptr<api::AwdlServerSocket> AwdlMedium::ListenForService(const api::PskInfo& psk_info,
                                                                    int port) {
  GNCNWFrameworkServerSocket* serverSocket =
      network_utils::ListenForService(medium_, psk_info, port, /*include_peer_to_peer=*/true);
  if (serverSocket != nil) {
    return std::make_unique<AwdlServerSocket>(serverSocket);
  }
  return nil;
}

}  // namespace apple
}  // namespace nearby
