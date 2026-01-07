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

#ifndef PLATFORM_IMPL_G3_WIFI_H_
#define PLATFORM_IMPL_G3_WIFI_H_

#include <string>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/medium_environment.h"

namespace nearby::g3 {

class WifiMedium;

// Container of operations that can be performed over the Wifi medium.
class WifiMedium : public api::WifiMedium {
 public:
  WifiMedium() {
    auto& env = MediumEnvironment::Instance();
    const std::string ip_addr_4bytes = env.GetFakeIPAddress();
    std::string ip_addr_dot_decimal;
    if (!ip_addr_4bytes.empty()) {
      for (auto byte : ip_addr_4bytes) {
        if (!ip_addr_dot_decimal.empty())
          absl::StrAppend(&ip_addr_dot_decimal, ".");
        absl::StrAppend(&ip_addr_dot_decimal, absl::StrFormat("%d", byte));
      }
    }

    wifi_capability_ = {true, false, true};
    wifi_information_ = {
      .is_connected = true,
      .ssid = "nearby_test_ap",
      .bssid = "34:36:3B:C7:8C:77",
      .ap_frequency = 5230,
    };
  }
  ~WifiMedium() override = default;

  WifiMedium(const WifiMedium&) = delete;
  WifiMedium(WifiMedium&&) = delete;
  WifiMedium& operator=(const WifiMedium&) = delete;
  WifiMedium& operator=(WifiMedium&&) = delete;

  // If the WiFi Adaptor supports to start a Wifi interface.
  bool IsInterfaceValid() const override { return true; }
  api::WifiCapability& GetCapability() override {
    absl::MutexLock lock(mutex_);
    return wifi_capability_;
  }
  api::WifiInformation& GetInformation() override {
    absl::MutexLock lock(mutex_);
    return wifi_information_;
  }

 private:
  absl::Mutex mutex_;
  api::WifiCapability wifi_capability_ ABSL_GUARDED_BY(mutex_);
  api::WifiInformation wifi_information_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby::g3

#endif  // PLATFORM_IMPL_G3_WIFI_H_
