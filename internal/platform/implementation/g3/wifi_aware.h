// Copyright 2026 Google LLC
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

#ifndef PLATFORM_IMPL_G3_WIFI_AWARE_H_
#define PLATFORM_IMPL_G3_WIFI_AWARE_H_

#include <memory>
#include <string>

#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/wifi_aware.h"
#include "internal/platform/wifi_aware_service_info.h"

namespace nearby::g3 {

// Container of operations that can be performed over the WifiAware medium.
class WifiAwareMedium : public api::WifiAwareMedium {
 public:
  WifiAwareMedium() = default;
  ~WifiAwareMedium() override = default;

  WifiAwareMedium(const WifiAwareMedium&) = delete;
  WifiAwareMedium(WifiAwareMedium&&) = delete;
  WifiAwareMedium& operator=(const WifiAwareMedium&) = delete;
  WifiAwareMedium& operator=(WifiAwareMedium&&) = delete;

  bool StartAdvertising(
      const WifiAwareServiceInfo& wifi_aware_service_info) override {
    return false;
  }
  bool StopAdvertising(
      const WifiAwareServiceInfo& wifi_aware_service_info) override {
    return false;
  }
  bool StartDiscovery(const std::string& service_type,
                      DiscoveredServiceCallback callback) override {
    return false;
  }
  bool StopDiscovery(const std::string& service_type) override { return false; }
  bool IsPublishing() override { return false; }
  bool StartPublishing() override { return false; }
  bool StopPublishing() override { return false; }
  bool IsSubscribing() override { return false; }
  bool StartSubscribing() override { return false; }
  bool StopSubscribing() override { return false; }
  std::unique_ptr<api::WifiAwareSocket> ConnectToService(
      const WifiAwareServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) override {
    return nullptr;
  }
  std::unique_ptr<api::WifiAwareServerSocket> ListenForService(
      int port) override {
    return nullptr;
  }
};

}  // namespace nearby::g3

#endif  // PLATFORM_IMPL_G3_WIFI_AWARE_H_
