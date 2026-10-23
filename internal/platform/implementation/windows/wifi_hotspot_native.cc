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

#include "internal/platform/implementation/windows/wifi_hotspot_native.h"

// clang-format off
#include <windows.h>
#include <winsock2.h>
#include <wlanapi.h>
#include <iphlpapi.h>
#include <cguid.h>
// clang-format on

#include <cstdint>
#include <cstring>
#include <cwchar>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/windows/network_info.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/wlan_client.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

namespace {
constexpr absl::Duration kConnectTimeout = absl::Seconds(15);
constexpr absl::string_view kHotspotProfileName = "_QS_peer_hotspot_";
constexpr char kProfileTemplate[] =
    R"(<?xml version="1.0"?>
<WLANProfile xmlns = "http://www.microsoft.com/networking/WLAN/profile/v1" >
<name>%s</name>
<SSIDConfig>
<SSID>
<name>%s</name>
</SSID>
</SSIDConfig>
<connectionType>ESS</connectionType>
<connectionMode>auto</connectionMode>
<MSM>
<security>
<authEncryption>
<authentication>WPA2PSK</authentication>
<encryption>AES</encryption>
<useOneX>false</useOneX>
</authEncryption>
<sharedKey>
<keyType>passPhrase</keyType>
<protected>false</protected>
<keyMaterial>%s</keyMaterial>
</sharedKey>
</security>
</MSM>
</WLANProfile>)";

std::string ReasonCodeToString(DWORD reason_code) {
  std::wstring reason_str;
  reason_str.resize(100);
  WlanReasonCodeToString(reason_code, reason_str.size(), reason_str.data(),
                         /*pReserved=*/nullptr);
  reason_str.resize(std::wcslen(reason_str.data()));
  return string_utils::WideStringToString(reason_str);
}

}  // namespace

WifiHotspotNative::WifiHotspotNative()
    : network_info_(NetworkInfo::GetNetworkInfo()) {
  if (!wlan_client_.Initialize()) {
    LOG(ERROR) << "Failed to initialize WLAN client.";
    return;
  }
  VLOG(1) << "WifiHotspotNative created successfully.";
  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid != GUID_NULL) {
    RemoveCreatedWlanProfile(interface_guid);
  }
}

WifiHotspotNative::~WifiHotspotNative() {
  VLOG(1) << "WifiHotspotNative destroyed successfully.";
}

bool WifiHotspotNative::ConnectToWifiNetwork(absl::string_view ssid,
                                             absl::string_view password) {
  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid == GUID_NULL) {
    LOG(ERROR) << __func__ << ": No available WLAN Interface to use.";
    return false;
  }
  {
    absl::MutexLock lock(mutex_);
    if (!SetWlanProfile(interface_guid, ssid, password)) {
      LOG(ERROR) << "Failed to set WLAN profile.";
      return false;
    }
    backup_profile_name_.clear();
    if (!ConnectToWifiNetworkInternal(interface_guid,
                                      std::string(kHotspotProfileName))) {
      RemoveCreatedWlanProfile(interface_guid);
      return false;
    }
  }
  LOG(ERROR) << "Connect to Wifi network successfully.";
  return true;
}

bool WifiHotspotNative::DisconnectWifiNetwork() {
  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid == GUID_NULL) {
    LOG(ERROR) << __func__ << ": No available WLAN Interface to use.";
    return false;
  }
  absl::MutexLock lock(mutex_);
  DWORD result = WlanDisconnect(wlan_client_.GetHandle(), &interface_guid,
                                /*pReserved=*/nullptr);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to disconnect WLAN profile with error: " << result;
  } else {
    LOG(ERROR) << "Disconnect Wifi hotspot successfully.";
  }
  return true;
}

void WifiHotspotNative::WlanNotificationCallback(
    PWLAN_NOTIFICATION_DATA wlan_notification_data, PVOID context) {
  VLOG(1) << "WlanNotificationCallback is called with notification code "
          << wlan_notification_data->NotificationCode;

  WlanNotificationContext* wlan_context =
      static_cast<WlanNotificationContext*>(context);
  switch (wlan_notification_data->NotificationCode) {
    case wlan_notification_acm_connection_start: {
      PWLAN_CONNECTION_NOTIFICATION_DATA data =
          static_cast<PWLAN_CONNECTION_NOTIFICATION_DATA>(
              wlan_notification_data->pData);
      wlan_context->got_connecting_event = true;
      LOG(INFO) << "Start connecting to profile "
                << string_utils::WideStringToString(
                       std::wstring(data->strProfileName));
      break;
    }
    case wlan_notification_acm_connection_complete: {
      // Make sure the connected WLAN profile is the one we set.
      PWLAN_CONNECTION_NOTIFICATION_DATA data =
          static_cast<PWLAN_CONNECTION_NOTIFICATION_DATA>(
              wlan_notification_data->pData);
      std::string profile_name =
          string_utils::WideStringToString(std::wstring(data->strProfileName));
      LOG(INFO) << "Completed connection to profile: " << profile_name
                << " code: " << data->wlanReasonCode;
      if (wlan_context->connecting_profile_name != profile_name) {
        break;
      }
      wlan_context->connection_code = data->wlanReasonCode;
      wlan_context->connection_completed.Notify();
      break;
    }
    case wlan_notification_acm_scan_list_refresh: {
      LOG(INFO) << "Scan list refreshed.";
      break;
    }
    case wlan_notification_acm_disconnecting: {
      // We use these events to get the profile name of the original network
      // the client is connected to so that we can restore it later.
      PWLAN_CONNECTION_NOTIFICATION_DATA data =
          static_cast<WLAN_CONNECTION_NOTIFICATION_DATA*>(
              wlan_notification_data->pData);
      // Ignore if we already started connecting to new network, the disconnect
      // event will not be the one we want.
      if (!wlan_context->got_connecting_event) {
        wlan_context->original_profile_name = string_utils::WideStringToString(
            std::wstring(data->strProfileName));
        LOG(INFO) << "Disconnecting from previous profile: "
                  << wlan_context->original_profile_name;
      }
      break;
    }
    default: {
      break;
    }
  }
}

std::wstring WifiHotspotNative::BuildWlanProfile(absl::string_view ssid,
                                                 absl::string_view password) {
  std::string profile =
      absl::StrFormat(kProfileTemplate, kHotspotProfileName, ssid, password);
  return string_utils::StringToWideString(profile);
}

GUID WifiHotspotNative::GetInterfaceGuid() const {
  if (wlan_client_.GetHandle() == nullptr) {
    return GUID_NULL;
  }

  // Find WLAN interface, only support one interface for now.
  std::vector<WlanClient::InterfaceInfo> interfaces =
      wlan_client_.GetInterfaceInfos();
  if (interfaces.empty()) {
    LOG(ERROR) << "No WLAN interfaces found.";
    return GUID_NULL;
  }
  return interfaces[0].guid;
}

bool WifiHotspotNative::ConnectToWifiNetworkInternal(
    GUID interface_guid, const std::string& profile_name) {
  if (profile_name.empty()) {
    LOG(ERROR) << "Profile name is empty.";
    return false;
  }
  absl::Time deadline = absl::Now() + kConnectTimeout;
  // Retry hotspot connection until timeout or error.
  while (deadline > absl::Now()) {
    WlanNotificationContext context = {
        .connecting_profile_name = profile_name,
    };
    if (!RegisterWlanNotificationCallback(&context)) {
      LOG(ERROR) << "Failed to register WLAN notification callback.";
      return false;
    }

    std::wstring wprofile_name = string_utils::StringToWideString(profile_name);
    WLAN_CONNECTION_PARAMETERS parameters;
    parameters.wlanConnectionMode = wlan_connection_mode_profile;
    parameters.strProfile = wprofile_name.data();
    parameters.pDot11Ssid = nullptr;
    parameters.pDesiredBssidList = nullptr;
    parameters.dot11BssType = dot11_BSS_type_infrastructure;
    parameters.dwFlags = 0;

    LOG(INFO) << "Begin connecting to WLAN profile " << profile_name;
    DWORD result = WlanConnect(wlan_client_.GetHandle(), &interface_guid,
                               &parameters, /*pReserved=*/nullptr);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to connect to WLAN profile " << profile_name;
      UnregisterWlanNotificationCallback();
      return false;
    }

    // Make sure that we connect to the expected profile.
    bool notified =
        context.connection_completed.WaitForNotificationWithDeadline(deadline);
    UnregisterWlanNotificationCallback();

    // Only backup original profile on first connection attempt and we did not
    // just disconnect from the hotspot.
    if (backup_profile_name_.empty() &&
        context.original_profile_name != kHotspotProfileName) {
      backup_profile_name_ = std::move(context.original_profile_name);
    }
    if (!notified) {
      break;
    }
    if (context.connection_code == WLAN_REASON_CODE_SUCCESS) {
      return true;
    }
    // If network is not found, try again.
    if (context.connection_code != WLAN_REASON_CODE_NETWORK_NOT_AVAILABLE &&
        context.connection_code != WLAN_REASON_CODE_USER_CANCELLED) {
      LOG(ERROR) << "Failed to connect to Wifi hotspot, code: "
                  << ReasonCodeToString(context.connection_code);
      return false;
    }
    if (context.connection_code == WLAN_REASON_CODE_USER_CANCELLED) {
      LOG(WARNING) << "Hotspot connection cancelled, trying again.";
    } else {
      LOG(WARNING) << "Hotspot not available, trying again.";
    }
    absl::SleepFor(absl::Seconds(1));
  }
  LOG(ERROR) << "Connect to Wifi hotspot timed out.";
  return false;
}

bool WifiHotspotNative::RegisterWlanNotificationCallback(
    WlanNotificationContext* absl_nonnull context) {
  DWORD result = WlanRegisterNotification(
      wlan_client_.GetHandle(), WLAN_NOTIFICATION_SOURCE_ACM,
      /*bIgnoreDuplicate=*/TRUE, WifiHotspotNative::WlanNotificationCallback,
      context, /*pReserved=*/nullptr,
      /*pdwPrevNotifSource=*/nullptr);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to register WLAN notification with error: " << result;
    return false;
  }

  return true;
}

bool WifiHotspotNative::UnregisterWlanNotificationCallback() {
  DWORD result = WlanRegisterNotification(
      wlan_client_.GetHandle(), WLAN_NOTIFICATION_SOURCE_NONE,
      /*bIgnoreDuplicate=*/TRUE,
      /*funcCallback=*/nullptr, /*pCallbackContext=*/nullptr,
      /*pReserved=*/nullptr, /*pdwPrevNotifSource=*/nullptr);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to unregister WLAN notification with error: "
               << result;
    return false;
  }

  return true;
}

bool WifiHotspotNative::SetWlanProfile(GUID interface_guid,
                                       absl::string_view ssid,
                                       absl::string_view password) {
  std::wstring profile = BuildWlanProfile(ssid, password);
  DWORD reason = 0;
  DWORD result =
      WlanSetProfile(wlan_client_.GetHandle(), &interface_guid,
                     WLAN_PROFILE_USER, profile.data(),
                     /*strAllUserProfileSecurity=*/nullptr, /*bOverwrite=*/TRUE,
                     /*pReserved=*/nullptr, /*pdwReasonCode*/ &reason);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set WLAN profile with error: " << result;
    if (result == ERROR_BAD_PROFILE) {
      std::wstring reason_str;
      reason_str.resize(100);
      WlanReasonCodeToString(reason, reason_str.size(), reason_str.data(),
                             /*pReserved=*/nullptr);
      reason_str.resize(std::wcslen(reason_str.data()) + 1);
      LOG(ERROR) << "WLAN profile error: " << reason_str;
    }
    return false;
  }

  LOG(INFO) << "Set WLAN profile to " << kHotspotProfileName
            << " successfully.";
  return true;
}

bool WifiHotspotNative::RemoveCreatedWlanProfile(GUID interface_guid) {
  return RemoveWlanProfile(interface_guid, std::string(kHotspotProfileName));
}

bool WifiHotspotNative::RemoveWlanProfile(GUID interface_guid,
                                          const std::string& profile_name) {
  if (profile_name.empty()) {
    return false;
  }
  std::wstring wprofile_name = string_utils::StringToWideString(profile_name);
  DWORD result = WlanDeleteProfile(wlan_client_.GetHandle(), &interface_guid,
                                   wprofile_name.data(), /*pReserved=*/nullptr);

  if (result != ERROR_SUCCESS && result != ERROR_NOT_FOUND) {
    LOG(ERROR) << "Failed to remove WLAN profile " << profile_name
               << " with reason " << result;
    return false;
  }

  LOG(INFO) << "WLAN profile " << profile_name << " removed successfully.";
  return true;
}

bool WifiHotspotNative::RestoreWifiProfile() {
  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid == GUID_NULL) {
    LOG(ERROR) << __func__ << ": No available WLAN Interface to use.";
    return false;
  }
  absl::MutexLock lock(mutex_);
  RemoveCreatedWlanProfile(interface_guid);
  if (backup_profile_name_.empty()) {
    LOG(ERROR) << "No backup WLAN profile to restore.";
    DWORD result = WlanDisconnect(wlan_client_.GetHandle(), &interface_guid,
                                  /*pReserved=*/nullptr);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to disconnect WLAN with error: " << result;
    } else {
      LOG(INFO) << "Disconnect Wifi hotspot successfully.";
    }
    return false;
  }
  return ConnectToWifiNetworkInternal(interface_guid, backup_profile_name_);
}

bool WifiHotspotNative::HasAssignedAddress(bool include_ipv6) {
  if (!network_info_.Refresh()) {
    return false;
  }
  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid == GUID_NULL) {
    LOG(ERROR) << __func__ << ": No available WLAN Interface to use.";
    return false;
  }
  NET_LUID luid;
  ConvertInterfaceGuidToLuid(&interface_guid, &luid);
  for (const NetworkInfo::InterfaceInfo& interface :
       network_info_.GetInterfaces()) {
    if (interface.luid.Value != luid.Value) {
      continue;
    }
    if (include_ipv6) {
      if (!interface.ipv6_addresses.empty()) {
        return true;
      }
    }
    for (const SocketAddress& address : interface.ipv4_addresses) {
      // We ignore APIPA addresses since we won't be able to connect using them.
      if (!address.IsV4LinkLocal()) {
        return true;
      }
    }
  }
  return false;
}

bool WifiHotspotNative::RenewIpv4Address() const {
  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid == GUID_NULL) {
    LOG(ERROR) << __func__ << ": No available WLAN Interface to use.";
    return false;
  }
  NET_LUID luid;
  if (ConvertInterfaceGuidToLuid(&interface_guid, &luid) == NO_ERROR) {
    return network_info_.RenewIpv4Address(luid);
  }
  LOG(ERROR) << "Failed to convert interface guid to luid.";
  return false;
}

std::optional<uint32_t> WifiHotspotNative::GetWifiInterfaceIndex() const {
  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid == GUID_NULL) {
    LOG(ERROR) << __func__ << ": No available WLAN Interface to use.";
    return std::nullopt;
  }
  NET_LUID luid;
  if (ConvertInterfaceGuidToLuid(&interface_guid, &luid) == NO_ERROR) {
    NET_IFINDEX interface_index = 0;
    if (ConvertInterfaceLuidToIndex(&luid, &interface_index) == NO_ERROR) {
      return interface_index;
    }
  }
  LOG(ERROR) << "Failed to convert interface guid to interface index.";
  return std::nullopt;
}

}  // namespace nearby::windows
