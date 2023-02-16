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

#ifndef PLATFORM_IMPL_APPLE_WIFI_LAN_H_
#define PLATFORM_IMPL_APPLE_WIFI_LAN_H_

#import <Foundation/Foundation.h>
#import <Network/Network.h>

#include <string>
#include <utility>

#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/nsd_service_info.h"

@class GNCMBonjourBrowser;
@class GNCMBonjourService;
@class GNCWiFiLANMedium;
@class GNCWiFiLANServerSocket;
@class GNCWiFiLANSocket;

namespace nearby {
namespace apple {

/**
 * InputStream that reads from GNCMConnection.
 */
class WifiLanInputStream : public InputStream {
 public:
  explicit WifiLanInputStream(GNCWiFiLANSocket* socket);
  ~WifiLanInputStream() override = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

 private:
  GNCWiFiLANSocket* socket_;
};

/**
 * OutputStream that writes to GNCMConnection.
 */
class WifiLanOutputStream : public OutputStream {
 public:
  explicit WifiLanOutputStream(GNCWiFiLANSocket* socket);
  ~WifiLanOutputStream() override = default;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  GNCWiFiLANSocket* socket_;
};

/**
 * Concrete WifiLanSocket implementation.
 */
class WifiLanSocket : public api::WifiLanSocket {
 public:
  explicit WifiLanSocket(GNCWiFiLANSocket* socket);
  ~WifiLanSocket() override = default;

  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  Exception Close() override;

 private:
  GNCWiFiLANSocket* socket_;
  std::unique_ptr<WifiLanInputStream> input_stream_;
  std::unique_ptr<WifiLanOutputStream> output_stream_;
};

/**
 * Concrete WifiLanServerSocket implementation.
 */
class WifiLanServerSocket : public api::WifiLanServerSocket {
 public:
  explicit WifiLanServerSocket(GNCWiFiLANServerSocket*  server_socket);
  ~WifiLanServerSocket() override = default;

  std::string GetIPAddress() const override;
  int GetPort() const override;
  std::unique_ptr<api::WifiLanSocket> Accept() override;
  Exception Close() override;

 private:
   GNCWiFiLANServerSocket* server_socket_;
};

/**
 * Concrete WifiLanMedium implementation.
 */
class WifiLanMedium : public api::WifiLanMedium {
 public:
  WifiLanMedium();
  ~WifiLanMedium() override = default;

  WifiLanMedium(const WifiLanMedium&) = delete;
  WifiLanMedium& operator=(const WifiLanMedium&) = delete;

  bool IsNetworkConnected() const override { return true; }
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() override {
    return absl::nullopt;
  }

  bool StartAdvertising(const NsdServiceInfo& nsd_service_info) override;
  bool StopAdvertising(const NsdServiceInfo& nsd_service_info) override;
  bool StartDiscovery(const std::string& service_type, DiscoveredServiceCallback callback) override;
  bool StopDiscovery(const std::string& service_type) override;
  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info, CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const std::string& ip_address, int port, CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::WifiLanServerSocket> ListenForService(int port) override;

 private:
  GNCWiFiLANMedium* medium_;
  absl::AnyInvocable<void(NsdServiceInfo)> service_discovered_cb_;
  absl::AnyInvocable<void(NsdServiceInfo)> service_lost_cb_;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_WIFI_LAN_H_
