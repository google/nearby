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

#import "internal/platform/implementation/apple/wifi_hotspot.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <string>
#include <utility>

#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCHotspotMedium.h"
#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCHotspotSocket.h"
#import "GoogleToolboxForMac/GTMLogger.h"
#include <arpa/inet.h>


namespace nearby {
namespace apple {

#pragma mark - WifiHotspotInputStream

WifiHotspotInputStream::WifiHotspotInputStream(GNCHotspotSocket* socket) : socket_(socket) {}

ExceptionOr<ByteArray> WifiHotspotInputStream::Read(std::int64_t size) {
  NSError* error = nil;
  NSData* data = [socket_ readMaxLength:size error:&error];
  if (data == nil) {
    GTMLoggerError(@"Error reading socket: %@", error);
    return {Exception::kIo};
  }
  return ExceptionOr<ByteArray>{ByteArray((const char*)data.bytes, data.length)};
}

Exception WifiHotspotInputStream::Close() {
  // The input stream reads directly from the connection. It can not be closed without closing the
  // connection itself. A call to `WifiHotspotSocket::Close` will close the connection.
  return {Exception::kSuccess};
}

#pragma mark - WifiHotspotOutputStream

WifiHotspotOutputStream::WifiHotspotOutputStream(GNCHotspotSocket* socket) : socket_(socket) {}

Exception WifiHotspotOutputStream::Write(const ByteArray& data) {
  NSError* error = nil;
  BOOL result = [socket_ write:[NSData dataWithBytes:data.data() length:data.size()] error:&error];
  if (!result) {
    GTMLoggerError(@"Error writing socket: %@", error);
    return {Exception::kIo};
  }
  return {Exception::kSuccess};
}

Exception WifiHotspotOutputStream::Flush() {
  // Write blocks until the data has successfully been written/received, so no more work is needed
  // to flush.
  return {Exception::kSuccess};
}

Exception WifiHotspotOutputStream::Close() {
  // The output stream writes directly to the connection. It can not be closed without closing the
  // connection itself. A call to `WifiHotspotSocket::Close` will close the connection.
  return {Exception::kSuccess};
}

#pragma mark - WifiHotspotSocket

WifiHotspotSocket::WifiHotspotSocket(GNCHotspotSocket* socket)
    : socket_(socket),
      input_stream_(std::make_unique<WifiHotspotInputStream>(socket)),
      output_stream_(std::make_unique<WifiHotspotOutputStream>(socket)) {}

InputStream& WifiHotspotSocket::GetInputStream() { return *input_stream_; }

OutputStream& WifiHotspotSocket::GetOutputStream() { return *output_stream_; }

Exception WifiHotspotSocket::Close() {
  [socket_ close];
  return {Exception::kSuccess};
}

#pragma mark - WifiHotspotMedium

WifiHotspotMedium::WifiHotspotMedium() : medium_([[GNCHotspotMedium alloc] init]) {} // NOLINT
WifiHotspotMedium::~WifiHotspotMedium() {
  DisconnectWifiHotspot();
}

bool WifiHotspotMedium::ConnectWifiHotspot(HotspotCredentials* hotspot_credentials_) {
  NSString* ssid = [[NSString alloc] initWithUTF8String:hotspot_credentials_->GetSSID().c_str()];
  NSString* password =
      [[NSString alloc] initWithUTF8String:hotspot_credentials_->GetPassword().c_str()];

  GTMLoggerInfo(@"ConnectWifiHotspot SSID: %@ Password: %@", ssid, password);
  bool result = [medium_ connectToWifiNetworkWithSSID:ssid
                         password:password];
  if (result) {
    GTMLoggerInfo(@"Successfully connected to %@", ssid);
    hotspot_ssid_ = ssid;
  } else {
    GTMLoggerError(@"Failed to connect to %@", ssid);
  }

  return result;
}

bool WifiHotspotMedium::DisconnectWifiHotspot() {
  GTMLoggerInfo(@"DisconnectWifiHotspot SSID: %@", hotspot_ssid_);
  if (hotspot_ssid_) {
    [medium_ disconnectToWifiNetworkWithSSID:hotspot_ssid_];
    hotspot_ssid_ = nil;
  }

  return true;
}

std::unique_ptr<api::WifiHotspotSocket> WifiHotspotMedium::ConnectToService(
    absl::string_view ip_address, int port, CancellationFlag* cancellation_flag) {
  NSError* error = nil;
  GNCIPv4Address* host;

  if (ip_address.size() == 4) {
    // 4 bytes IP address format.
    NSData* host_ip_address = [NSData dataWithBytes:ip_address.data() length:ip_address.size()];
    host = [GNCIPv4Address addressFromData:host_ip_address];
  } else {
    // Dot-decimal IP address format.
    // Convert the dot-decimal IP address string to a network byte order integer.
    struct in_addr host_ip_address;
    host_ip_address.s_addr = inet_addr(ip_address.data());

    if (host_ip_address.s_addr == INADDR_NONE) {
        GTMLoggerError(@"Invalid IP address: %.*s\n", (int)ip_address.size(), ip_address.data());
        return nil;
    }
    host = [GNCIPv4Address addressFromFourByteInt:host_ip_address.s_addr];
  }
  GTMLoggerInfo(@"Connect to Hotspot host server: %@", [host dottedRepresentation]);

  GNCHotspotSocket* socket = [medium_ connectToHost:host port:port error:&error];
  if (socket != nil) {
    return std::make_unique<WifiHotspotSocket>(socket);
  }
  if (error != nil) {
    GTMLoggerError(@"Error connecting to %@:%d: %@", host, port, error);
  }
  return nil;
}
}  // namespace apple
}  // namespace nearby
