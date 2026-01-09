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

#include <arpa/inet.h>
#include "internal/base/masker.h"
#include "internal/platform/cancellation_flag_listener.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/Hotspot/GNCHotspotMedium.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCIPv4Address.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFramework.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkServerSocket.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFrameworkSocket.h"
#import "internal/platform/implementation/apple/network_utils.h"

namespace nearby {
namespace apple {
namespace {
constexpr char kHotspotQueueLabel[] = "com.google.nearby.wifi_hotspot";
}

#pragma mark - WifiHotspotInputStream

WifiHotspotInputStream::WifiHotspotInputStream(GNCNWFrameworkSocket* socket) : socket_(socket) {}

ExceptionOr<ByteArray> WifiHotspotInputStream::Read(std::int64_t size) {
  NSError* error = nil;
  NSData* data = [socket_ readMaxLength:size error:&error];
  if (data == nil) {
    GNCLoggerError(@"Error reading socket: %@", error);
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

WifiHotspotOutputStream::WifiHotspotOutputStream(GNCNWFrameworkSocket* socket) : socket_(socket) {}

Exception WifiHotspotOutputStream::Write(absl::string_view data) {
  NSError* error = nil;
  BOOL result = [socket_ write:[NSData dataWithBytes:data.data() length:data.size()] error:&error];
  if (!result) {
    GNCLoggerError(@"Error writing socket: %@", error);
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

WifiHotspotSocket::WifiHotspotSocket(GNCNWFrameworkSocket* socket)
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

WifiHotspotMedium::WifiHotspotMedium() {
  hotspot_queue_ = dispatch_queue_create(kHotspotQueueLabel, DISPATCH_QUEUE_SERIAL);
  medium_ = [[GNCHotspotMedium alloc] initWithQueue:hotspot_queue_];
}

WifiHotspotMedium::WifiHotspotMedium(GNCHotspotMedium* hotspot_medium) : medium_(hotspot_medium) {}

WifiHotspotMedium::~WifiHotspotMedium() { DisconnectWifiHotspot(); }

bool WifiHotspotMedium::ConnectWifiHotspot(const HotspotCredentials& hotspot_credentials_) {
  NSString* ssid = [[NSString alloc] initWithUTF8String:hotspot_credentials_.GetSSID().c_str()];
  std::string hotspot_password = hotspot_credentials_.GetPassword();
  NSString* password = [[NSString alloc] initWithUTF8String:hotspot_password.c_str()];

  GNCLoggerInfo(@"ConnectWifiHotspot SSID: %@ Password: %s", ssid,
                masker::Mask(hotspot_password).c_str());
  bool result = [medium_ connectToWifiNetworkWithSSID:ssid password:password];
  if (result) {
    GNCLoggerInfo(@"Successfully connected to %@", ssid);
    hotspot_ssid_ = ssid;
  } else {
    GNCLoggerError(@"Failed to connect to %@", ssid);
  }

  return result;
}

bool WifiHotspotMedium::DisconnectWifiHotspot() {
  GNCLoggerInfo(@"DisconnectWifiHotspot SSID: %@", hotspot_ssid_);
  if (hotspot_ssid_) {
    [medium_ disconnectToWifiNetworkWithSSID:hotspot_ssid_];
    hotspot_ssid_ = nil;
  }

  return true;
}

std::unique_ptr<api::WifiHotspotSocket> WifiHotspotMedium::ConnectToService(
    const ServiceAddress& service_address, CancellationFlag* cancellation_flag) {
  NSError* error = nil;
  GNCIPv4Address* host;

  // Only supports IPv4 address.
  if (service_address.address.size() != 4) {
    GNCLoggerError(@"Invalid IP address size: %lu", service_address.address.size());
    return nullptr;
  }
  // 4 bytes IP address format.
  NSData* host_ip_address = [NSData dataWithBytes:service_address.address.data()
                                            length:service_address.address.size()];
  host = [GNCIPv4Address addressFromData:host_ip_address];
  GNCLoggerInfo(@"Connect to Hotspot host server: %@", [host dottedRepresentation]);

  // Setup cancel listener
  std::unique_ptr<nearby::CancellationFlagListener> connection_cancellation_listener = nullptr;
  dispatch_source_t cancellation_source = nullptr;
  if (cancellation_flag != nullptr) {
    if (cancellation_flag->Cancelled()) {
      return nullptr;
    }

    cancellation_source =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0, hotspot_queue_);

    dispatch_resume(cancellation_source);
    connection_cancellation_listener = std::make_unique<nearby::CancellationFlagListener>(
        cancellation_flag, [&cancellation_source]() {
          GNCLoggerWarning(@"Cancelling to connect WiFi hotspot.");
          dispatch_source_merge_data(cancellation_source, 1);
        });
  }

  GNCNWFrameworkSocket* socket = [medium_ connectToHost:host
                                                   port:service_address.port
                                           cancelSource:cancellation_source
                                                  error:&error];
  if (socket != nil) {
    return std::make_unique<WifiHotspotSocket>(socket);
  }
  if (error != nil) {
    GNCLoggerError(@"Error connecting to %@:%d: %@", host, service_address.port, error);
  }
  return nil;
}
}  // namespace apple
}  // namespace nearby
