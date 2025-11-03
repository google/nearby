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

#ifndef PLATFORM_IMPL_APPLE_WIFI_HOTSPOT_H_
#define PLATFORM_IMPL_APPLE_WIFI_HOTSPOT_H_

#import <Foundation/Foundation.h>  // NOLINT
#import <Network/Network.h>

#include <string>
#include <utility>

#include "internal/platform/implementation/wifi_hotspot.h"

@class GNCMBonjourBrowser;
@class GNCMBonjourService;
@class GNCHotspotMedium;
@class GNCNWFrameworkSocket;
@class GNCNWFrameworkServerSocket;

namespace nearby {
namespace apple {

/**
 * InputStream that reads from GNCMConnection.
 */
class WifiHotspotInputStream : public InputStream {
 public:
  explicit WifiHotspotInputStream(GNCNWFrameworkSocket* socket);
  ~WifiHotspotInputStream() override = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

 private:
  GNCNWFrameworkSocket* socket_;
};

/**
 * OutputStream that writes to GNCMConnection.
 */
class WifiHotspotOutputStream : public OutputStream {
 public:
  explicit WifiHotspotOutputStream(GNCNWFrameworkSocket* socket);
  ~WifiHotspotOutputStream() override = default;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  GNCNWFrameworkSocket* socket_;
};

/**
 * Concrete WifiHotspotSocket implementation.
 */
class WifiHotspotSocket : public api::WifiHotspotSocket {
 public:
  explicit WifiHotspotSocket(GNCNWFrameworkSocket* socket);
  ~WifiHotspotSocket() override = default;
  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  Exception Close() override;

 private:
  GNCNWFrameworkSocket* const socket_;
  std::unique_ptr<WifiHotspotInputStream> input_stream_;
  std::unique_ptr<WifiHotspotOutputStream> output_stream_;
};

/**
 * Concrete WifiHotspotMedium implementation.
 */
class WifiHotspotMedium : public api::WifiHotspotMedium {
 public:
  WifiHotspotMedium();
  // For testing only.
  explicit WifiHotspotMedium(GNCHotspotMedium* hotspot_medium);
  ~WifiHotspotMedium() override;

  WifiHotspotMedium(const WifiHotspotMedium&) = delete;
  WifiHotspotMedium& operator=(const WifiHotspotMedium&) = delete;

  /**
   * @brief Check if the Wifi interface is valid.
   */
  bool IsInterfaceValid() const override { return true; }

  /**
   * @brief Connects to a Wifi Hotspot.
   *
   * @param hotspot_credentials_ The credentials of the Wifi Hotspot to connect to.
   */
  bool ConnectWifiHotspot(
      const location::nearby::connections::BandwidthUpgradeNegotiationFrame::UpgradePathInfo::
          WifiHotspotCredentials& hotspot_credentials_) override;

  /**
   * @brief Disconnects from the currently connected Wifi Hotspot.
   */
  bool DisconnectWifiHotspot() override;

  /**
   * @brief Connects to a Wifi Hotspot service.
   *
   * @param ip_address The IP address of the service.
   * @param port The port of the service.
   * @param cancellation_flag A flag to cancel the connection.
   * @return A unique pointer to a WifiHotspotSocket if the connection was successful, nullptr
   * otherwise.
   */
  std::unique_ptr<api::WifiHotspotSocket> ConnectToService(
      absl::string_view ip_address, int port, CancellationFlag* cancellation_flag) override;

  // IOS is not supporting WifiHotspot Server yet.
  bool StartWifiHotspot(location::nearby::connections::BandwidthUpgradeNegotiationFrame::
                            UpgradePathInfo::WifiHotspotCredentials* hotspot_credentials) override {
    return false;
  }
  bool StopWifiHotspot() override { return false; }
  std::unique_ptr<api::WifiHotspotServerSocket> ListenForService(int port) override {
    return nullptr;
  }

  /**
   * @brief Gets the dynamic port range for the Wifi Hotspot.
   *
   * @return An optional pair of integers representing the minimum and maximum dynamic port numbers,
   * or absl::nullopt if not supported.
   */
  std::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() override {
    return std::nullopt;
  }

 private:
  dispatch_queue_t hotspot_queue_ = nil;
  GNCHotspotMedium* medium_;
  NSString* hotspot_ssid_ = nil;
};
}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_WIFI_HOTSPOT_H_
