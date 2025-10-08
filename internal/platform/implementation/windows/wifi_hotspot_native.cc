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
#include <cguid.h>
// clang-format on

#include <cstring>
#include <cwchar>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"

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
}  // namespace

WifiHotspotNative::WifiHotspotNative() {
  // Open WLAN handle
  DWORD negotiated_version;
  DWORD result = WlanOpenHandle(/*dwClientVersion=*/2, /*pReserved=*/nullptr,
                                /*pdwNegotiatedVersion=*/&negotiated_version,
                                /*phClientHandle=*/&wifi_);

  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to open WLAN handle.";
    return;
  }

  VLOG(1) << "WifiHotspotNative created successfully.";
  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid != GUID_NULL) {
    RemoveCreatedWlanProfile(interface_guid);
  }
}

WifiHotspotNative::~WifiHotspotNative() {
  if (wifi_ != nullptr) {
    WlanCloseHandle(wifi_, nullptr);
    wifi_ = nullptr;
  }

  VLOG(1) << "WifiHotspotNative destroyed successfully.";
}

bool WifiHotspotNative::ConnectToWifiNetwork(
    HotspotCredentials* hotspot_credentials) {
  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid == GUID_NULL) {
    LOG(ERROR) << "No available WLAN Interface to use.";
    return false;
  }
  {
    absl::MutexLock lock(mutex_);
    if (!SetWlanProfile(interface_guid, hotspot_credentials)) {
      LOG(ERROR) << "Failed to set WLAN profile.";
      return false;
    }
    backup_profile_name_.clear();
    if (!ConnectToWifiNetworkInternal(interface_guid,
                                      string_utils::StringToWideString(
                                          std::string(kHotspotProfileName)))) {
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
    LOG(ERROR) << "No available WLAN Interface to use.";
    return false;
  }
  absl::MutexLock lock(mutex_);
  DWORD result = WlanDisconnect(
      /*hClientHandle=*/wifi_, /*pInterfaceGuid=*/&interface_guid,
      /*pReserved=*/nullptr);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to disconnect WLAN profile with error: " << result;
  } else {
    LOG(ERROR) << "Disconnect Wifi hotspot successfully.";
  }
  return true;
}

bool WifiHotspotNative::Scan(absl::string_view ssid) {
  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid == GUID_NULL) {
    LOG(ERROR) << "No available WLAN Interface to use.";
    return false;
  }
  WlanNotificationContext context = {
    .wifi_hotspot_native = *this,
  };
  absl::MutexLock lock(mutex_);
  if (!RegisterWlanNotificationCallback(&context)) {
    LOG(ERROR) << "Failed to register WLAN notification callback.";
    return false;
  }

  if (ssid.length() > DOT11_SSID_MAX_LENGTH) {
    LOG(ERROR) << "Invalid SSID length.";
    return false;
  }

  DOT11_SSID dot11_ssid;
  dot11_ssid.uSSIDLength = ssid.length();
  memcpy(dot11_ssid.ucSSID, ssid.data(), dot11_ssid.uSSIDLength);
  scan_latch_ = std::make_unique<CountDownLatch>(1);
  scanning_ssid_ = ssid;
  DWORD result = WlanScan(
      /*hClientHandle=*/wifi_, /*pInterfaceGuid=*/&interface_guid,
      /*pDot11Ssid=*/&dot11_ssid, /*pIeData=*/nullptr, /*pReserved=*/nullptr);

  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to scan Wi-Fi network with error " << result;
    UnregisterWlanNotificationCallback();
    return false;
  }

  ExceptionOr<bool> scan_result = scan_latch_->Await(kConnectTimeout);
  UnregisterWlanNotificationCallback();

  if (!scan_result.ok() || !scan_result.result()) {
    LOG(ERROR) << "Failed to scan to Wifi network " << ssid;
    return false;
  }

  scan_latch_ = nullptr;
  return true;
}

void WifiHotspotNative::TriggerConnected() {
  if (connect_latch_ == nullptr) {
    return;
  }
  connect_latch_->CountDown();
}

void WifiHotspotNative::TriggerNetworkRefreshed() {
  if (scan_latch_ == nullptr) {
    return;
  }

  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid == GUID_NULL) {
    LOG(ERROR) << "No available WLAN Interface to use.";
    return;
  }

  PWLAN_AVAILABLE_NETWORK_LIST list = nullptr;
  DWORD result = WlanGetAvailableNetworkList(
      /*hClientHandle=*/wifi_, /*pInterfaceGuid=*/&interface_guid,
      /*dwFlags=*/WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_ADHOC_PROFILES,
      /*pReserved=*/nullptr, /*ppAvailableNetworkList=*/&list);
  if (result != ERROR_SUCCESS) {
    return;
  }

  bool found_ssid = false;
  for (int i = 0; i < list->dwNumberOfItems; i++) {
    std::string ssid = std::string((char*)list->Network[i].dot11Ssid.ucSSID,
                                   list->Network[i].dot11Ssid.uSSIDLength);
    if (ssid == scanning_ssid_) {
      found_ssid = true;
      break;
    }
  }

  WlanFreeMemory(list);

  if (found_ssid) {
    LOG(INFO) << "Found WLAN network " << scanning_ssid_;
    scan_latch_->CountDown();
  }
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
      if (wlan_context->connecting_profile_name != data->strProfileName) {
        break;
      }
      wlan_context->connection_code = data->wlanReasonCode;
      wlan_context->wifi_hotspot_native.TriggerConnected();
      break;
    }
    case wlan_notification_acm_scan_list_refresh: {
      LOG(INFO) << "Scan list refreshed.";
      wlan_context->wifi_hotspot_native.TriggerNetworkRefreshed();
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
        wlan_context->original_profile_name =
            std::wstring(data->strProfileName);
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
  if (wifi_ == nullptr) {
    return GUID_NULL;
  }

  // Find WLAN interface, only support one interface for now.
  PWLAN_INTERFACE_INFO_LIST interfaces = nullptr;
  DWORD result =
      WlanEnumInterfaces(/*hClientHandle=*/wifi_, /*pReserved=*/nullptr,
                         /*ppInterfaceList=*/&interfaces);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to enum WLAN interfaces.";
    return GUID_NULL;
  }

  if (interfaces->dwNumberOfItems == 0) {
    LOG(ERROR) << "No WLAN interfaces found.";
    WlanFreeMemory(interfaces);
    return GUID_NULL;
  }

  GUID interface_guid = interfaces->InterfaceInfo[0].InterfaceGuid;
  WlanFreeMemory(interfaces);
  return interface_guid;
}

bool WifiHotspotNative::ConnectToWifiNetworkInternal(
    GUID interface_guid, const std::wstring& profile_name) {
  if (profile_name.empty()) {
    LOG(ERROR) << "Profile name is empty.";
    return false;
  }
  WlanNotificationContext context = {
    .wifi_hotspot_native = *this,
    .connecting_profile_name = profile_name,
  };
  if (!RegisterWlanNotificationCallback(&context)) {
    LOG(ERROR) << "Failed to register WLAN notification callback.";
    return false;
  }

  connect_latch_ = std::make_unique<CountDownLatch>(1);

  WLAN_CONNECTION_PARAMETERS parameters;
  parameters.wlanConnectionMode = wlan_connection_mode_profile;
  parameters.strProfile = profile_name.data();
  parameters.pDot11Ssid = nullptr;
  parameters.pDesiredBssidList = nullptr;
  parameters.dot11BssType = dot11_BSS_type_infrastructure;
  parameters.dwFlags = 0;

  DWORD result =
      WlanConnect(/*hClientHandle=*/wifi_, /*pInterfaceGuid=*/&interface_guid,
                  /*pConnectionParameters=*/&parameters, /*pReserved=*/nullptr);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to connect to WLAN profile "
               << string_utils::WideStringToString(profile_name);
    UnregisterWlanNotificationCallback();
    return false;
  }

  // Make sure that we connect to the expected profile.
  ExceptionOr<bool> connect_result = connect_latch_->Await(kConnectTimeout);
  UnregisterWlanNotificationCallback();

  if (!connect_result.ok() || !connect_result.result()) {
    LOG(ERROR) << "Connect to Wifi hotspot timed out.";
    return false;
  }
  if (context.connection_code != WLAN_REASON_CODE_SUCCESS) {
    LOG(ERROR) << "Failed to connect to Wifi hotspot, code: "
               << context.connection_code;
    return false;
  }

  connect_latch_ = nullptr;
  backup_profile_name_ = std::move(context.original_profile_name);
  return true;
}

bool WifiHotspotNative::RegisterWlanNotificationCallback(
    WlanNotificationContext* absl_nonnull context) {
  DWORD result = WlanRegisterNotification(
      /*hClientHandle=*/wifi_, /*dwNotifSource=*/WLAN_NOTIFICATION_SOURCE_ACM,
      /*bIgnoreDuplicate=*/TRUE,
      /*funcCallback=*/WifiHotspotNative::WlanNotificationCallback,
      /*pCallbackContext=*/context, /*pReserved=*/nullptr,
      /*pdwPrevNotifSource=*/nullptr);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to register WLAN notification with error: " << result;
    return false;
  }

  return true;
}

bool WifiHotspotNative::UnregisterWlanNotificationCallback() {
  DWORD result = WlanRegisterNotification(
      /*hClientHandle=*/wifi_, /*dwNotifSource=*/WLAN_NOTIFICATION_SOURCE_NONE,
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

bool WifiHotspotNative::SetWlanProfile(
    GUID interface_guid, HotspotCredentials* hotspot_credentials) {
  std::wstring profile = BuildWlanProfile(hotspot_credentials->GetSSID(),
                                          hotspot_credentials->GetPassword());
  DWORD reason = 0;
  DWORD result = WlanSetProfile(
      /*hClientHandle=*/wifi_, /*pInterfaceGuid=*/&interface_guid,
      /*dwFlags=*/WLAN_PROFILE_USER, /*strProfileXml=*/profile.data(),
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
  return RemoveWlanProfile(interface_guid,
          string_utils::StringToWideString(std::string(kHotspotProfileName)));
}

bool WifiHotspotNative::RemoveWlanProfile(GUID interface_guid,
                                          const std::wstring& profile_name) {
  if (profile_name.empty()) {
    return false;
  }
  DWORD result = WlanDeleteProfile(
      /*hClientHandle=*/wifi_, /*pInterfaceGuid=*/&interface_guid,
      /*strProfileName=*/profile_name.data(), /*pReserved=*/nullptr);

  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to remove WLAN profile "
               << string_utils::WideStringToString(profile_name)
               << " with reason " << result;
    return false;
  }

  LOG(INFO) << "WLAN profile "
            << string_utils::WideStringToString(profile_name)
            << " removed successfully.";
  return true;
}

bool WifiHotspotNative::RestoreWifiProfile() {
  GUID interface_guid = GetInterfaceGuid();
  if (interface_guid == GUID_NULL) {
    LOG(ERROR) << "No available WLAN Interface to use.";
    return false;
  }
  absl::MutexLock lock(mutex_);
  RemoveCreatedWlanProfile(interface_guid);
  if (backup_profile_name_.empty()) {
    LOG(ERROR) << "No backup WLAN profile to restore.";
    DWORD result = WlanDisconnect(
        /*hClientHandle=*/wifi_, /*pInterfaceGuid=*/&interface_guid,
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

}  // namespace nearby::windows
