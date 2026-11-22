// Copyright 2024 Google LLC
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

#ifndef PLATFORM_IMPL_ANDROID_WIFI_LAN_H_
#define PLATFORM_IMPL_ANDROID_WIFI_LAN_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace android {

class WifiLanMedium;

class WifiLanSocket : public api::WifiLanSocket {
 public:
  ~WifiLanSocket() override = default;

  InputStream& GetInputStream() override;

  OutputStream& GetOutputStream() override;

  Exception Close() override;
};

class WifiLanServerSocket : public api::WifiLanServerSocket {
 public:
  ~WifiLanServerSocket() override;

  std::string GetIPAddress() const override;

  int GetPort() const override;

  std::unique_ptr<api::WifiLanSocket> Accept() override;

  Exception Close() override;

 private:
  mutable absl::Mutex mutex_;
};

class WifiLanMedium : public api::WifiLanMedium {
 public:
  WifiLanMedium();
  ~WifiLanMedium() override;

  bool IsNetworkConnected() const override;

  bool StartAdvertising(const NsdServiceInfo& nsd_service_info) override;

  bool StopAdvertising(const NsdServiceInfo& nsd_service_info) override;

  bool StartDiscovery(const std::string& service_type,
                      DiscoveredServiceCallback callback) override;

  bool StopDiscovery(const std::string& service_type) override;

  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) override;

  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const std::string& ip_address, int port,
      CancellationFlag* cancellation_flag) override;

  std::unique_ptr<api::WifiLanServerSocket> ListenForService(int port) override;

  std::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override;

 private:
  absl::Mutex mutex_;
};

}  // namespace android
}  // namespace nearby

#endif  // PLATFORM_IMPL_ANDROID_WIFI_LAN_H_
