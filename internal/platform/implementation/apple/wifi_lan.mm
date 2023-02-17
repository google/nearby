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

#import "internal/platform/implementation/apple/wifi_lan.h"

#include <memory>
#include <string>
#include <utility>

#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCWiFiLANMedium.h"
#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCWiFiLANServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCWiFiLANSocket.h"
#import "GoogleToolboxForMac/GTMLogger.h"

namespace nearby {
namespace apple {

#pragma mark - WifiLanInputStream

WifiLanInputStream::WifiLanInputStream(GNCWiFiLANSocket* socket) : socket_(socket) {}

ExceptionOr<ByteArray> WifiLanInputStream::Read(std::int64_t size) {
  NSError* error = nil;
  NSData* data = [socket_ readMaxLength:size error:&error];
  if (data == nil) {
    GTMLoggerError(@"Error reading socket: %@", error);
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

WifiLanOutputStream::WifiLanOutputStream(GNCWiFiLANSocket* socket) : socket_(socket) {}

Exception WifiLanOutputStream::Write(const ByteArray& data) {
  NSError* error = nil;
  BOOL result = [socket_ write:[NSData dataWithBytes:data.data() length:data.size()] error:&error];
  if (!result) {
    GTMLoggerError(@"Error writing socket: %@", error);
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

WifiLanSocket::WifiLanSocket(GNCWiFiLANSocket* socket)
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

WifiLanServerSocket::WifiLanServerSocket(GNCWiFiLANServerSocket* server_socket)
    : server_socket_(server_socket) {}

std::string WifiLanServerSocket::GetIPAddress() const {
  return [server_socket_.ipAddress UTF8String];
}

int WifiLanServerSocket::GetPort() const { return server_socket_.port; }

std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept() {
  NSError* error = nil;
  GNCWiFiLANSocket* socket = [server_socket_ acceptWithError:&error];
  if (socket != nil) {
    return std::make_unique<WifiLanSocket>(socket);
  }
  if (error != nil) {
    GTMLoggerError(@"Error accepting socket: %@", error);
  }
  return nil;
}

Exception WifiLanServerSocket::Close() {
  [server_socket_ close];
  return {Exception::kSuccess};
}

#pragma mark - WifiLanMedium

WifiLanMedium::WifiLanMedium() : medium_([[GNCWiFiLANMedium alloc] init]) {}

bool WifiLanMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  NSInteger port = nsd_service_info.GetPort();
  NSString* serviceName = @(nsd_service_info.GetServiceName().c_str());
  NSString* serviceType = @(nsd_service_info.GetServiceType().c_str());
  NSMutableDictionary<NSString*, NSString*>* txtRecords = [[NSMutableDictionary alloc] init];
  for (const auto& record : nsd_service_info.GetTxtRecords()) {
    [txtRecords setObject:@(record.second.c_str()) forKey:@(record.first.c_str())];
  }
  [medium_ startAdvertisingPort:port
                    serviceName:serviceName
                    serviceType:serviceType
                     txtRecords:txtRecords];
  return true;
}

bool WifiLanMedium::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  NSInteger port = nsd_service_info.GetPort();
  [medium_ stopAdvertisingPort:port];
  return true;
}

bool WifiLanMedium::StartDiscovery(const std::string& service_type,
                                   DiscoveredServiceCallback callback) {
  __block NSString* serviceType = @(service_type.c_str());
  __block DiscoveredServiceCallback client_callback = std::move(callback);

  NSError* error = nil;
  BOOL result = [medium_ startDiscoveryForServiceType:serviceType
      serviceFoundHandler:^(NSString* name, NSDictionary<NSString*, NSString*>* txtRecords) {
        NsdServiceInfo nsd_service_info;
        nsd_service_info.SetServiceType([serviceType UTF8String]);
        nsd_service_info.SetServiceName([name UTF8String]);
        [txtRecords
            enumerateKeysAndObjectsUsingBlock:[nsd_service_info = &nsd_service_info](
                                                  NSString* key, NSString* val, BOOL* stop) {
              nsd_service_info->SetTxtRecord([key UTF8String], [val UTF8String]);
            }];
        client_callback.service_discovered_cb(nsd_service_info);
      }
      serviceLostHandler:^(NSString* name, NSDictionary<NSString*, NSString*>* txtRecords) {
        NsdServiceInfo nsd_service_info;
        nsd_service_info.SetServiceType([serviceType UTF8String]);
        nsd_service_info.SetServiceName([name UTF8String]);
        [txtRecords
            enumerateKeysAndObjectsUsingBlock:[nsd_service_info = &nsd_service_info](
                                                  NSString* key, NSString* val, BOOL* stop) {
              nsd_service_info->SetTxtRecord([key UTF8String], [val UTF8String]);
            }];
        client_callback.service_lost_cb(nsd_service_info);
      }
      error:&error];
  if (error != nil) {
    GTMLoggerError(@"Error starting discovery for service type<%@>: %@", serviceType, error);
  }
  return result;
}

bool WifiLanMedium::StopDiscovery(const std::string& service_type) {
  NSString* serviceType = @(service_type.c_str());
  [medium_ stopDiscoveryForServiceType:serviceType];
  return true;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info, CancellationFlag* cancellation_flag) {
  NSError* error = nil;
  NSString* serviceName = @(remote_service_info.GetServiceName().c_str());
  NSString* serviceType = @(remote_service_info.GetServiceType().c_str());
  GNCWiFiLANSocket* socket = [medium_ connectToServiceName:serviceName
                                               serviceType:serviceType
                                                     error:&error];
  if (socket != nil) {
    return std::make_unique<WifiLanSocket>(socket);
  }
  if (error != nil) {
    GTMLoggerError(@"Error connecting to service name<%@> type<%@>: %@", serviceName, serviceType,
                   error);
  }
  return nil;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const std::string& ip_address, int port, CancellationFlag* cancellation_flag) {
  NSError* error = nil;
  NSString* host = @(ip_address.c_str());
  GNCWiFiLANSocket* socket = [medium_ connectToHost:host port:port error:&error];
  if (socket != nil) {
    return std::make_unique<WifiLanSocket>(socket);
  }
  if (error != nil) {
    GTMLoggerError(@"Error connecting to %@:%d: %@", host, port, error);
  }
  return nil;
}

std::unique_ptr<api::WifiLanServerSocket> WifiLanMedium::ListenForService(int port) {
  NSError* error = nil;
  GNCWiFiLANServerSocket* serverSocket = [medium_ listenForServiceOnPort:port error:&error];
  if (serverSocket != nil) {
    return std::make_unique<WifiLanServerSocket>(serverSocket);
  }
  if (error != nil) {
    GTMLoggerError(@"Error listening for service: %@", error);
  }
  return nil;
}

}  // namespace apple
}  // namespace nearby
