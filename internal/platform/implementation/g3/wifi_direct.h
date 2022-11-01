// Copyright 2022 Google LLC
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

#ifndef PLATFORM_IMPL_G3_WIFI_DIRECT_H_
#define PLATFORM_IMPL_G3_WIFI_DIRECT_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/g3/multi_thread_executor.h"
#include "internal/platform/implementation/g3/pipe.h"
#include "internal/platform/implementation/g3/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace location {
namespace nearby {
namespace g3 {

class WifiDirectMedium;

// Container of operations that can be performed over the WifiDirect medium.
class WifiDirectMedium : public api::WifiDirectMedium {
 public:
  WifiDirectMedium();
  ~WifiDirectMedium() override;

  WifiDirectMedium(const WifiDirectMedium&) = delete;
  WifiDirectMedium(WifiDirectMedium&&) = delete;
  WifiDirectMedium& operator=(const WifiDirectMedium&) = delete;
  WifiDirectMedium& operator=(WifiDirectMedium&&) = delete;

  // If the WiFi Adaptor supports to start a WifiDirect interface.
  bool IsInterfaceValid() const override { return true; }

  // Discoverer connects to server socket
  std::unique_ptr<api::WifiHotspotSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag) override;

  // Advertiser starts to listen on server socket
  std::unique_ptr<api::WifiHotspotServerSocket> ListenForService(
      int port) override;

  // Advertiser start WiFiDirect GO with specific Crendentials
  bool StartWifiDirect(HotspotCredentials* wifi_direct_credentials) override;
  // Advertiser stop the current WiFiDirect GO
  bool StopWifiDirect() override;
  // Discoverer connects to the WiFiDirect GO
  bool ConnectWifiDirect(HotspotCredentials* wifi_direct_credentials) override;
  // Discoverer disconnects from the WiFiDirect GO
  bool DisconnectWifiDirect() override;

  std::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override {
    return std::nullopt;
  }

 private:
  absl::Mutex mutex_;

  absl::flat_hash_map<std::string, WifiHotspotServerSocket*> server_sockets_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_G3_WIFI_DIRECT_H_
