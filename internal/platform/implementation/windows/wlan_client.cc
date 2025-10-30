// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/windows/wlan_client.h"

#include <windows.h>
#include <wlanapi.h>

#include <vector>

#include "internal/platform/implementation/windows/scoped_wlan_memory.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

WlanClient::~WlanClient() {
  if (wifi_ != nullptr) {
    WlanCloseHandle(wifi_, /*pReserved=*/nullptr);
  }
}

bool WlanClient::Initialize() {
  if (wifi_ != nullptr) {
    return true;
  }
  DWORD client_version = 2;
  DWORD negotiated_version = 0;
  DWORD result = 0;

  result = WlanOpenHandle(client_version, /*pReserved=*/nullptr,
                          &negotiated_version, &wifi_);
  if (result == ERROR_SUCCESS) {
    return true;
  }
  LOG(INFO) << "WlanOpenHandle failed with error: " << result;
  return false;
}

std::vector<WlanClient::InterfaceInfo> WlanClient::PopulateInterfaceInfos(
    PWLAN_INTERFACE_INFO_LIST interfaces) const {
  if (interfaces == nullptr) {
    return {};
  }
  std::vector<InterfaceInfo> interface_infos;
  for (int i = 0; i < interfaces->dwNumberOfItems; ++i) {
    bool connected =
        interfaces->InterfaceInfo[i].isState == wlan_interface_state_connected;
    bool ready =
        interfaces->InterfaceInfo[i].isState != wlan_interface_state_not_ready;
    interface_infos.push_back(
        {connected, ready, interfaces->InterfaceInfo[i].InterfaceGuid});
  }
  return interface_infos;
}

std::vector<WlanClient::InterfaceInfo> WlanClient::GetInterfaceInfos() const {
  if (wifi_ == nullptr) {
    LOG(ERROR) << "WlanClient is not initialized.";
    return {};
  }
  ScopedWlanMemory<WLAN_INTERFACE_INFO_LIST> interfaces;
  DWORD result =
      WlanEnumInterfaces(wifi_, /*pReserved=*/nullptr, interfaces.OutParam());
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to enum WLAN interfaces.";
    return {};
  }
  return PopulateInterfaceInfos(interfaces.get());
}

}  // namespace nearby::windows
