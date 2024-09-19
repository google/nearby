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

#ifndef PLATFORM_IMPL_WINDOWS_WIFI_H_
#define PLATFORM_IMPL_WINDOWS_WIFI_H_

#include <objbase.h>
#include <windows.h>
#include <wlanapi.h>
#include <wtypes.h>

// Nearby connections headers
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/implementation/wifi_utils.h"

// WinRT headers
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Connectivity.h"

namespace nearby {
namespace windows {

using ::winrt::Windows::Networking::HostName;
using ::winrt::Windows::Networking::HostNameType;
using ::winrt::Windows::Networking::Connectivity::ConnectionProfile;
using ::winrt::Windows::Networking::Connectivity::NetworkInformation;

// Represents a WiFi network found during a call to WifiMedium#scan().
class WifiScanResult : public api::WifiScanResult {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~WifiScanResult() override = default;

  // Gets the SSID of this WiFi network.
  // TODO(b/184975123): replace with real implementation.
  std::string GetSsid() const override { return "Un-implemented"; }
  // Gets the signal strength of this WiFi network in dBm.
  // TODO(b/184975123): replace with real implementation.
  std::int32_t GetSignalStrengthDbm() const override { return 0; }
  // Gets the frequency band of this WiFi network in MHz.
  // TODO(b/184975123): replace with real implementation.
  std::int32_t GetFrequencyMhz() const override { return 0; }
  // Gets the authentication type of this WiFi network.
  // TODO(b/184975123): replace with real implementation.
  api::WifiAuthType GetAuthType() const override {
    return api::WifiAuthType::kUnknown;
  }
};

// Container of operations that can be performed over the WiFi medium.
class WifiMedium : public api::WifiMedium {
 public:
  WifiMedium();
  ~WifiMedium() override = default;

  bool IsInterfaceValid() const override;

  api::WifiCapability& GetCapability() override { return wifi_capability_; }

  api::WifiInformation& GetInformation() override;

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

  // Returns the local device's IP address in the IPv4 dotted-quad format.
  // TODO(b/184975123): replace with real implementation.
  std::string GetIpAddress() override;

 private:
  // Since the WiFi interface capability won't change in the connection session,
  // we only need to query it once at the beginning
  void InitCapability();
  std::string InternalGetWifiIpAddress();
  std::string InternalGetEthernetIpAddress();
  void FillupEthernetParams();

  bool wifi_interface_valid_;
  api::WifiCapability wifi_capability_;
  api::WifiInformation wifi_information_;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_H_
