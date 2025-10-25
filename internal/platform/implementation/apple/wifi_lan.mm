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

#import "internal/platform/implementation/apple/wifi_lan.h"
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

#pragma mark - WifiLanInputStream

WifiLanInputStream::WifiLanInputStream(GNCNWFrameworkSocket* socket) : socket_(socket) {}

ExceptionOr<ByteArray> WifiLanInputStream::Read(std::int64_t size) {
  NSError* error = nil;
  NSData* data = [socket_ readMaxLength:size error:&error];
  if (data == nil) {
    GNCLoggerError(@"Error reading socket: %@", error);
    return {Exception::kIo};
  }
  return ExceptionOr<ByteArray>{ByteArray((const char*)data.bytes, data.length)};
}

Exception WifiLanInputStream::Close() {
  // The input stream reads directly from the connection. It can not be closed without closing the
  // connection itself. A call to `WifiLanSocket::Close` will close the connection.
  return {Exception::kSuccess};
}

#pragma mark - WifiLanOutputStream

WifiLanOutputStream::WifiLanOutputStream(GNCNWFrameworkSocket* socket) : socket_(socket) {}

Exception WifiLanOutputStream::Write(const ByteArray& data) {
  NSError* error = nil;
  BOOL result = [socket_ write:[NSData dataWithBytes:data.data() length:data.size()] error:&error];
  if (!result) {
    GNCLoggerError(@"Error writing socket: %@", error);
    return {Exception::kIo};
  }
  return {Exception::kSuccess};
}

Exception WifiLanOutputStream::Flush() {
  // Write blocks until the data has successfully been written/received, so no more work is needed
  // to flush.
  return {Exception::kSuccess};
}

Exception WifiLanOutputStream::Close() {
  // The output stream writes directly to the connection. It can not be closed without closing the
  // connection itself. A call to `WifiLanSocket::Close` will close the connection.
  return {Exception::kSuccess};
}

#pragma mark - WifiLanSocket

WifiLanSocket::WifiLanSocket(GNCNWFrameworkSocket* socket)
    : socket_(socket),
      input_stream_(std::make_unique<WifiLanInputStream>(socket)),
      output_stream_(std::make_unique<WifiLanOutputStream>(socket)) {}

InputStream& WifiLanSocket::GetInputStream() { return *input_stream_; }

OutputStream& WifiLanSocket::GetOutputStream() { return *output_stream_; }

Exception WifiLanSocket::Close() {
  [socket_ close];
  return {Exception::kSuccess};
}

#pragma mark - WifiLanServerSocket

WifiLanServerSocket::WifiLanServerSocket(GNCNWFrameworkServerSocket* server_socket)
    : server_socket_(server_socket) {}

std::string WifiLanServerSocket::GetIPAddress() const {
  NSData* addressData = server_socket_.ipAddress.binaryRepresentation;
  return std::string((char*)addressData.bytes, addressData.length);
}

int WifiLanServerSocket::GetPort() const { return server_socket_.port; }

std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept() {
  NSError* error = nil;
  GNCNWFrameworkSocket* socket = [server_socket_ acceptWithError:&error];
  if (socket != nil) {
    return std::make_unique<WifiLanSocket>(socket);
  }
  if (error != nil) {
    GNCLoggerError(@"Error accepting socket: %@", error);
  }
  return nil;
}

Exception WifiLanServerSocket::Close() {
  [server_socket_ close];
  return {Exception::kSuccess};
}

#pragma mark - WifiLanMedium

WifiLanMedium::WifiLanMedium() { medium_ = [[GNCNWFramework alloc] init]; }

WifiLanMedium::WifiLanMedium(GNCNWFramework* medium) : medium_(medium) {}

bool WifiLanMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  return network_utils::StartAdvertising(medium_, nsd_service_info);
}

bool WifiLanMedium::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  return network_utils::StopAdvertising(medium_, nsd_service_info);
}

bool WifiLanMedium::StartDiscovery(const std::string& service_type,
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

  return network_utils::StartDiscovery(medium_, service_type, std::move(network_callback), false);
}

bool WifiLanMedium::StopDiscovery(const std::string& service_type) {
  return network_utils::StopDiscovery(medium_, service_type);
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info, CancellationFlag* cancellation_flag) {
  GNCNWFrameworkSocket* socket =
      network_utils::ConnectToService(medium_, remote_service_info, cancellation_flag);
  if (socket != nil) {
    return std::make_unique<WifiLanSocket>(socket);
  }
  return nil;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const std::string& ip_address, int port, CancellationFlag* cancellation_flag) {
  GNCNWFrameworkSocket* socket = network_utils::ConnectToService(
      medium_, ip_address, port, /*include_peer_to_peer=*/false, cancellation_flag);
  if (socket != nil) {
    return std::make_unique<WifiLanSocket>(socket);
  }
  return nil;
}

std::unique_ptr<api::WifiLanServerSocket> WifiLanMedium::ListenForService(int port) {
  GNCNWFrameworkServerSocket* serverSocket =
      network_utils::ListenForService(medium_, port, /*include_peer_to_peer=*/false);
  if (serverSocket != nil) {
    return std::make_unique<WifiLanServerSocket>(serverSocket);
  }
  return nil;
}

std::vector<std::string> WifiLanMedium::GetUpgradeAddressCandidates(
    const api::WifiLanServerSocket& server_socket) {
  return { server_socket.GetIPAddress() };
}

}  // namespace apple
}  // namespace nearby
