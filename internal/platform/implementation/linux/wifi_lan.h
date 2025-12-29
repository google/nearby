// Copyright 2023 Google LLC
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

#ifndef PLATFORM_IMPL_LINUX_WIFI_LAN_H_
#define PLATFORM_IMPL_LINUX_WIFI_LAN_H_
#include <sdbus-c++/IConnection.h>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/avahi.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby {
namespace linux {
class WifiLanMedium : public api::WifiLanMedium {
 public:
  explicit WifiLanMedium(std::shared_ptr<networkmanager::NetworkManager> network_manager);

  bool IsNetworkConnected() const override;

  bool StartAdvertising(const NsdServiceInfo &nsd_service_info) override
      ABSL_LOCKS_EXCLUDED(entry_groups_mutex_);
  bool StopAdvertising(const NsdServiceInfo &nsd_service_info) override
      ABSL_LOCKS_EXCLUDED(entry_groups_mutex_);

  bool StartDiscovery(const std::string &service_type,
                      DiscoveredServiceCallback callback) override
      ABSL_LOCKS_EXCLUDED(service_browsers_mutex_);
  bool StopDiscovery(const std::string &service_type) override
      ABSL_LOCKS_EXCLUDED(service_browsers_mutex_);

  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const NsdServiceInfo &remote_service_info,
      CancellationFlag *cancellation_flag) override {
    return ConnectToService(remote_service_info.GetIPAddress(),
                            remote_service_info.GetPort(), cancellation_flag);
  };
  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const std::string &ip_address, int port,
      CancellationFlag *cancellation_flag) override;
  std::unique_ptr<api::WifiLanServerSocket> ListenForService(
      int port = 0) override;
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override {
    return std::nullopt;
  }

 private:
  std::shared_ptr<sdbus::IConnection> system_bus_;  
  std::shared_ptr<networkmanager::NetworkManager> network_manager_;

  std::shared_ptr<avahi::Server> avahi_;

  absl::Mutex entry_groups_mutex_;
  absl::flat_hash_map<std::pair<std::string, std::string>,
                      std::unique_ptr<avahi::EntryGroup>>
      entry_groups_ ABSL_GUARDED_BY(entry_groups_mutex_);

  absl::Mutex service_browsers_mutex_;
  absl::flat_hash_map<std::string, std::unique_ptr<avahi::ServiceBrowser>>
      service_browsers_ ABSL_GUARDED_BY(service_browsers_mutex_);
};
}  // namespace linux
}  // namespace nearby

#endif
