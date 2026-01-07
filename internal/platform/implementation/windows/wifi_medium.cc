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

#include <vector>

#include "internal/platform/implementation/wifi.h"
#include "internal/platform/implementation/windows/scoped_wlan_memory.h"
#include "internal/platform/implementation/windows/wifi.h"
#include "internal/platform/implementation/windows/wlan_client.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

namespace {
using ::nearby::windows::ScopedWlanMemory;
}  // namespace

WifiMedium::WifiMedium() { InitCapability(); }

bool WifiMedium::IsInterfaceValid() const { return wifi_interface_valid_; }

void WifiMedium::InitCapability() {
  wifi_capability_.supports_5_ghz = false;
  wifi_capability_.supports_6_ghz = false;

  WlanClient wlan_client;
  if (!wlan_client.Initialize()) {
    return;
  }
  std::vector<WlanClient::InterfaceInfo> interface_infos =
      wlan_client.GetInterfaceInfos();
  for (const auto& interface_info : interface_infos) {
    ScopedWlanMemory<WLAN_INTERFACE_CAPABILITY> intf_capability;
    wifi_interface_valid_ = true;
    if (WlanGetInterfaceCapability(
            wlan_client.GetHandle(), &interface_info.guid, nullptr,
            intf_capability.OutParam()) != ERROR_SUCCESS) {
      LOG(INFO) << "Get Capability failed";
      return;
    }

    for (int j = 0; j < intf_capability->dwNumberOfSupportedPhys; j++) {
      if (intf_capability->dot11PhyTypes[j] == dot11_phy_type_ofdm)
        wifi_capability_.supports_5_ghz = true;
    }
  }
}

}  // namespace nearby::windows
