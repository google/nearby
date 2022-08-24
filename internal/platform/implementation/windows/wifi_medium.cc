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

// #include <cstddef>
#include "absl/strings/str_format.h"
#include "internal/platform/implementation/windows/wifi.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

namespace location {
namespace nearby {
namespace windows {

namespace {
  constexpr int kMacAddrLen = 6;
  constexpr int kDefaultApFreq = -1;
}  // namespace


WifiMedium::WifiMedium() {
  InitCapability();
}

PWLAN_INTERFACE_INFO_LIST EnumInterface(PHANDLE client_handle) {
  DWORD client_version = 2;
  DWORD negotiated_version = 0;
  DWORD result = 0;

  /* variables used for WlanEnumInterfaces */
  PWLAN_INTERFACE_INFO_LIST p_intf_list = NULL;

  result =
      WlanOpenHandle(client_version, NULL, &negotiated_version, client_handle);
  if (result != ERROR_SUCCESS) {
    NEARBY_LOGS(INFO) << "WlanOpenHandle failed with error: " << result;
    return p_intf_list;
  }

  result = WlanEnumInterfaces(*client_handle, NULL, &p_intf_list);
  if (result != ERROR_SUCCESS) {
    NEARBY_LOGS(INFO) << "WlanEnumInterfaces failed with error: " << result;
  }
  return p_intf_list;
}

bool WifiMedium::IsInterfaceValid() const {
  return wifi_interface_valid_;
}

void WifiMedium::InitCapability() {
  HANDLE client_handle = NULL;

  /* variables used for WlanEnumInterfaces */
  PWLAN_INTERFACE_INFO_LIST p_intf_list = NULL;
  PWLAN_INTERFACE_INFO p_intf_info = NULL;
  PWLAN_INTERFACE_CAPABILITY p_intf_capability = NULL;

  wifi_capability_.supports_5_ghz = false;
  wifi_capability_.supports_6_ghz = false;
  wifi_interface_valid_ = false;

  p_intf_list = EnumInterface(&client_handle);
  if (!client_handle) {
    NEARBY_LOGS(INFO) << "Client Handle is NULL";
    return;
  }
  if (!p_intf_list) {
    NEARBY_LOGS(INFO) << "WlanEnumInterfaces failed with error: ";
    WlanCloseHandle(client_handle, NULL);
    return;
  }
  wifi_interface_valid_ = true;

  for (int i = 0; i < (int)p_intf_list->dwNumberOfItems; i++) {
    p_intf_info = (WLAN_INTERFACE_INFO*)&p_intf_list->InterfaceInfo[i];
    if (WlanGetInterfaceCapability(client_handle, &p_intf_info->InterfaceGuid,
                                   NULL, &p_intf_capability) != ERROR_SUCCESS) {
      NEARBY_LOGS(INFO) << "Get Capability failed";
      WlanFreeMemory(p_intf_list);
      p_intf_list = NULL;
      WlanCloseHandle(client_handle, NULL);
      return;
    }

    for (int j = 0; j < p_intf_capability->dwNumberOfSupportedPhys; j++) {
      if (p_intf_capability->dot11PhyTypes[j] == dot11_phy_type_ofdm)
        wifi_capability_.supports_5_ghz = true;
    }
  }

  WlanFreeMemory(p_intf_capability);
  p_intf_capability = NULL;
  WlanFreeMemory(p_intf_list);
  p_intf_list = NULL;
  WlanCloseHandle(client_handle, NULL);
}

api::WifiInformation& WifiMedium::GetInformation() {
  HANDLE client_handle = NULL;
  PWLAN_AVAILABLE_NETWORK_LIST pWLAN_AVAILABLE_NETWORK_LIST = NULL;
  DWORD result = 0;
  DWORD connect_info_size = sizeof(WLAN_CONNECTION_ATTRIBUTES);
  WLAN_OPCODE_VALUE_TYPE op_code_value_type = wlan_opcode_value_type_invalid;

  /* variables used for WlanEnumInterfaces */
  PWLAN_INTERFACE_INFO_LIST p_intf_list = NULL;
  PWLAN_INTERFACE_INFO p_intf_info = NULL;
  PWLAN_CONNECTION_ATTRIBUTES p_connect_info = NULL;
  ULONG* channel = NULL;

  wifi_information_.is_connected = false;
  wifi_information_.ap_frequency = kDefaultApFreq;

  p_intf_list = EnumInterface(&client_handle);
  if (!client_handle) {
    NEARBY_LOGS(INFO) << "Client Handle is NULL";
    return wifi_information_;
  }
  if (!p_intf_list) {
    NEARBY_LOGS(INFO) << "WlanEnumInterfaces failed with error: ";
    WlanCloseHandle(client_handle, NULL);
    return wifi_information_;
  }

  for (int i = 0; i < (int)p_intf_list->dwNumberOfItems; i++) {
    p_intf_info = (WLAN_INTERFACE_INFO*)&p_intf_list->InterfaceInfo[i];
    if (p_intf_info->isState == wlan_interface_state_connected) {
      NEARBY_LOGS(INFO) << "Found connected WiFi interface No: " << i;
      wifi_information_.is_connected = true;

      DWORD channel_size;
      result = WlanQueryInterface(client_handle, &p_intf_info->InterfaceGuid,
                                  wlan_intf_opcode_channel_number, NULL,
                                  &channel_size, (PVOID*)&channel,
                                  &op_code_value_type);
      if (result != ERROR_SUCCESS) {
        NEARBY_LOGS(INFO) << "WlanQueryInterface error = " << result;
      } else {
        wifi_information_.ap_frequency =
            WifiUtils::ConvertChannelToFrequencyMhz(
                *channel, api::WifiBandType::kUnknown);
        NEARBY_LOGS(INFO) << "Frequency: " << *channel << "; ap_frequency: "
                          << wifi_information_.ap_frequency;
        WlanFreeMemory(channel);
        channel = NULL;
      }

      result = WlanQueryInterface(client_handle, &p_intf_info->InterfaceGuid,
                                  wlan_intf_opcode_current_connection, NULL,
                                  &connect_info_size, (PVOID*)&p_connect_info,
                                  &op_code_value_type);
      if (result != ERROR_SUCCESS) {
        NEARBY_LOGS(INFO) << "WlanQueryInterface error = " << result;
        WlanFreeMemory(p_intf_list);
        p_intf_list = NULL;
        WlanCloseHandle(client_handle, NULL);
        return wifi_information_;
      }

      wifi_information_.ssid.resize(
          p_connect_info->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
      std::memcpy(
          wifi_information_.ssid.data(),
          reinterpret_cast<const char*>(
              p_connect_info->wlanAssociationAttributes.dot11Ssid.ucSSID),
          wifi_information_.ssid.size());
      NEARBY_LOGS(INFO) << "wifi ssid is: " << wifi_information_.ssid
                        << "; length is:" << wifi_information_.ssid.length();

      char str_tmp[kMacAddrLen];
      strncpy(str_tmp,
              reinterpret_cast<const char*>(
                  p_connect_info->wlanAssociationAttributes.dot11Bssid),
              kMacAddrLen);
      wifi_information_.bssid = absl::StrFormat(
          "%2llx:%2llx:%2llx:%2llx:%2llx:%2llx", str_tmp[0], str_tmp[1],
          str_tmp[2], str_tmp[3], str_tmp[4], str_tmp[5]);
      NEARBY_LOGS(INFO) << "wifi bssid is: " << wifi_information_.bssid;
    }
  }

  WlanFreeMemory(p_connect_info);
  p_connect_info = NULL;
  WlanFreeMemory(p_intf_list);
  p_intf_list = NULL;
  WlanCloseHandle(client_handle, NULL);

  wifi_information_.ip_address_dot_decimal = GetIpAddress();
  wifi_information_.ip_address_4_bytes = ipaddr_dotdecimal_to_4bytes_string(
      wifi_information_.ip_address_dot_decimal);

  return wifi_information_;
}

std::string WifiMedium::GetIpAddress() {
  auto host_names = NetworkInformation::GetHostNames();
  for (auto host_name : host_names) {
    if (host_name.IPInformation() != nullptr &&
        host_name.IPInformation().NetworkAdapter() != nullptr &&
        host_name.Type() == HostNameType::Ipv4) {
      std::string ipv4_s = winrt::to_string(host_name.ToString());
      NEARBY_LOGS(INFO) << "Found IP: " << ipv4_s;

      auto profile = host_name.IPInformation()
                         .NetworkAdapter()
                         .GetConnectedProfileAsync()
                         .get();
      if (profile.IsWlanConnectionProfile()) {
        if ( wifi_information_.ssid == winrt::to_string(
                   profile.WlanConnectionProfileDetails().GetConnectedSsid())) {
          NEARBY_LOGS(INFO)
              << "SSID of this IP matches with this WiFi interface's SSID:"
              << wifi_information_.ssid << ", return this IP";
          return ipv4_s;
        }
      }
    }
  }
  return {};
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
