// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_WIFI_HOTSPOT_NATIVE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_WIFI_HOTSPOT_NATIVE_H_

// clang-format off
#include <stdbool.h>
#include <windows.h>
#include <winsock2.h>
#include <wlanapi.h>
// clang-format on

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/implementation/windows/network_info.h"
#include "internal/platform/wifi_credential.h"

namespace nearby::windows {

class WifiHotspotNative {
 public:
  WifiHotspotNative();
  ~WifiHotspotNative();
  bool ConnectToWifiNetwork(HotspotCredentials* hotspot_credentials)
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool DisconnectWifiNetwork() ABSL_LOCKS_EXCLUDED(mutex_);

  bool RestoreWifiProfile() ABSL_LOCKS_EXCLUDED(mutex_);

  bool Scan(absl::string_view ssid) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if the interface has a non local scoped IPv4 address.
  bool HasAssignedAddress();
  bool RenewIpv4Address() const;

 private:
  // Context for WLAN notification callback.
  struct WlanNotificationContext {
    WifiHotspotNative& wifi_hotspot_native;
    // This is reset to false when we start connecting to a new network, and set
    // to true when we receive the connecting event.
    bool got_connecting_event = false;
    // Original profile name is captured when a disconnecting event is received
    // before a connecting event.
    std::wstring original_profile_name;
    // The profile name of the network we are trying to connect to.
    std::wstring connecting_profile_name;
    WLAN_REASON_CODE connection_code = WLAN_REASON_CODE_UNKNOWN;
  };

  static void WlanNotificationCallback(
      PWLAN_NOTIFICATION_DATA wlan_notification_data, PVOID context);
  std::wstring BuildWlanProfile(absl::string_view ssid,
                                absl::string_view password);
  GUID GetInterfaceGuid() const;
  bool ConnectToWifiNetworkInternal(GUID interface_guid,
                                    const std::wstring& profile_name)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool RegisterWlanNotificationCallback(
      WlanNotificationContext* absl_nonnull context)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool UnregisterWlanNotificationCallback();
  bool SetWlanProfile(GUID interface_guid,
                      HotspotCredentials* hotspot_credentials)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool RemoveCreatedWlanProfile(GUID interface_guid)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool RemoveWlanProfile(GUID interface_guid, const std::wstring& profile_name)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void TriggerConnected();
  void TriggerNetworkRefreshed();

  mutable absl::Mutex mutex_;

  HANDLE wifi_ = nullptr;
  std::string scanning_ssid_;
  std::unique_ptr<CountDownLatch> connect_latch_;
  std::unique_ptr<CountDownLatch> scan_latch_;
  std::wstring backup_profile_name_;
  NetworkInfo network_info_;
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_WIFI_HOTSPOT_NATIVE_H_
