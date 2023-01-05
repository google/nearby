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
#include <vector>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace g3 {

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
    wifi_information_ = {true, "nearby_test_ap",       "34:36:3B:C7:8C:77",
                         5230, ip_addr_dot_decimal, ip_addr_4bytes};
  }
  ~WifiMedium() override = default;

  WifiMedium(const WifiMedium&) = delete;
  WifiMedium(WifiMedium&&) = delete;
  WifiMedium& operator=(const WifiMedium&) = delete;
  WifiMedium& operator=(WifiMedium&&) = delete;

  // If the WiFi Adaptor supports to start a Wifi interface.
  bool IsInterfaceValid() const override { return true; }
  api::WifiCapability& GetCapability() override {
    absl::MutexLock lock(&mutex_);
    return wifi_capability_;
  }
  api::WifiInformation& GetInformation() override {
    absl::MutexLock lock(&mutex_);
    return wifi_information_;
  }
  std::string GetIpAddress() override {
    absl::MutexLock lock(&mutex_);
    return wifi_information_.ip_address_dot_decimal;
  }

  class ScanResultCallback : public api::WifiMedium::ScanResultCallback {
   public:
    // TODO(b/184975123): replace with real implementation.
    ~ScanResultCallback() override = default;

    // TODO(b/184975123): replace with real implementation.
    void OnScanResults(
        const std::vector<api::WifiScanResult>& scan_results) override {}
  };

  // Does not take ownership of the passed-in scan_result_callback -- destroying
  // that is up to the caller.
  // TODO(b/184975123): replace with real implementation.
  bool Scan(const api::WifiMedium::ScanResultCallback& scan_result_callback)
      override {
    return false;
  }

  // If 'password' is an empty string, none has been provided. Returns
  // WifiConnectionStatus::CONNECTED on success, or the appropriate failure code
  // otherwise.
  // TODO(b/184975123): replace with real implementation.
  api::WifiConnectionStatus ConnectToNetwork(
      absl::string_view ssid, absl::string_view password,
      api::WifiAuthType auth_type) override {
    return api::WifiConnectionStatus::kUnknown;
  }

  // Blocks until it's certain of there being a connection to the internet, or
  // returns false if it fails to do so.
  //
  // How this method wants to verify said connection is totally up to it (so it
  // can feel free to ping whatever server, download whatever resource, etc.
  // that it needs to gain confidence that the internet is reachable hereon in).
  // TODO(b/184975123): replace with real implementation.
  bool VerifyInternetConnectivity() override { return false; }

 private:
  absl::Mutex mutex_;
  api::WifiCapability wifi_capability_ ABSL_GUARDED_BY(mutex_);
  api::WifiInformation wifi_information_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_WIFI_H_
