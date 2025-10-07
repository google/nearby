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

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace windows {

class WifiHotspotNative {
 public:
  WifiHotspotNative();
  ~WifiHotspotNative();
  bool ConnectToWifiNetwork(HotspotCredentials* hotspot_credentials)
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool ConnectToWifiNetwork(const std::wstring& profile_name)
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool DisconnectWifiNetwork() ABSL_LOCKS_EXCLUDED(mutex_);

  std::optional<std::wstring> GetConnectedProfileName() const
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool Scan(absl::string_view ssid) ABSL_LOCKS_EXCLUDED(mutex_);

  void TriggerConnected(const std::wstring& profile_name);
  void TriggerNetworkRefreshed();

 private:
  static void WlanNotificationCallback(
      PWLAN_NOTIFICATION_DATA wlan_notification_data, PVOID context);
  std::wstring BuildWlanProfile(absl::string_view ssid,
                                absl::string_view password);
  GUID GetInterfaceGuid() const;
  bool ConnectToWifiNetworkInternal(const std::wstring& profile_name);
  bool RegisterWlanNotificationCallback();
  bool UnregisterWlanNotificationCallback();
  bool SetWlanProfile(HotspotCredentials* hotspot_credentials);
  bool RemoveCreatedWlanProfile() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  bool RemoveWlanProfile(const std::wstring& profile_name)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  std::optional<std::wstring> GetConnectedProfileNameInternal() const;

  mutable absl::Mutex mutex_;

  HANDLE wifi_ = nullptr;
  std::wstring connecting_profile_name_;
  std::string scanning_ssid_;
  std::unique_ptr<CountDownLatch> connect_latch_;
  std::unique_ptr<CountDownLatch> scan_latch_;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_WIFI_HOTSPOT_NATIVE_H_
