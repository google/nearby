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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_WLAN_CLIENT_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_WLAN_CLIENT_H_

#include <windows.h>
#include <wlanapi.h>
#include <coguid.h>

#include <vector>

namespace nearby::windows {

// A wrapper for Windows wlan client handle and some APIs.
class WlanClient {
 public:
  struct InterfaceInfo {
    // Whether the interface is connected to an AP.
    bool connected = false;
    // Whether the interface is ready to use.
    bool ready = false;
    // Interface GUID of the WLAN interface.
    GUID guid = GUID_NULL;
  };

  WlanClient() = default;
  ~WlanClient();

  // Initializes the wlan client handle. Returns true if successful.
  // If the client has already been initialized, this is a no-op.
  bool Initialize();

  // Returns the wlan client handle.
  // If the client has not been initialized, this returns nullptr.
  HANDLE GetHandle() const { return wifi_; }

  // Returns the state of all WLAN interfaces.
  // If the client has not been initialized, this returns an empty vector.
  std::vector<InterfaceInfo> GetInterfaceInfos() const;

 private:
  friend class WlanClientTest;

  std::vector<InterfaceInfo> PopulateInterfaceInfos(
      PWLAN_INTERFACE_INFO_LIST interfaces) const;

  HANDLE wifi_ = nullptr;
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_WLAN_CLIENT_H_
