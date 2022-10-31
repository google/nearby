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

#ifndef PLATFORM_PUBLIC_WIFI_DIRECT_H_
#define PLATFORM_PUBLIC_WIFI_DIRECT_H_

#include <memory>
#include <string>
#include <utility>

#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/wifi_hotspot.h"
#include "internal/platform/wifi_hotspot_credential.h"

namespace location {
namespace nearby {

// Container of operations that can be performed over the WifiLan medium.
class WifiDirectMedium {
 public:
  using Platform = api::ImplementationPlatform;

  WifiDirectMedium() : impl_(Platform::CreateWifiDirectMedium()) {}
  ~WifiDirectMedium() = default;

  // Returns a new WifiDirectSocket by ip address and port.
  // On Success, WifiDirectSocket::IsValid() returns true.
  WifiHotspotSocket ConnectToService(absl::string_view ip_address, int port,
                                     CancellationFlag* cancellation_flag);

  // Returns a new WifiDirectServerSocket.
  // On Success, WifiDirectServerSocket::IsValid() returns true.
  WifiHotspotServerSocket ListenForService(int port = 0) {
    return WifiHotspotServerSocket(impl_->ListenForService(port));
  }

  // Returns the port range as a pair of min and max port.
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() {
    return impl_->GetDynamicPortRange();
  }

  bool StartWifiDirect() {
    MutexLock lock(&mutex_);
    return impl_->StartWifiDirect(&wifi_direct_credentials_);
  }
  bool StopWifiDirect() { return impl_->StopWifiDirect(); }

  bool ConnectWifiDirect(absl::string_view ssid, absl::string_view password) {
    MutexLock lock(&mutex_);
    wifi_direct_credentials_.SetSSID(std::string(ssid));
    wifi_direct_credentials_.SetPassword(std::string(password));
    return impl_->ConnectWifiDirect(&wifi_direct_credentials_);
  }
  bool DisconnectWifiDirect() { return impl_->DisconnectWifiDirect(); }

  HotspotCredentials* GetCredential() { return &wifi_direct_credentials_; }

  bool IsInterfaceValid() const {
    CHECK(impl_);
    return impl_->IsInterfaceValid();
  }

  bool IsValid() const { return impl_ != nullptr; }
  api::WifiDirectMedium& GetImpl() {
    CHECK(impl_);
    return *impl_;
  }

 private:
  Mutex mutex_;
  std::unique_ptr<api::WifiDirectMedium> impl_;
  HotspotCredentials wifi_direct_credentials_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_WIFI_DIRECT_H_
