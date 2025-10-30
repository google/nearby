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

#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/implementation/wifi_utils.h"
#include "internal/platform/implementation/windows/scoped_wlan_memory.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/implementation/windows/wifi.h"
#include "internal/platform/implementation/windows/wlan_client.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Security.Authorization.AppCapabilityAccess.h"

namespace nearby {
namespace windows {

namespace {
using ::nearby::windows::ScopedWlanMemory;
using winrt::Windows::Security::Authorization::AppCapabilityAccess::
    AppCapability;
using winrt::Windows::Security::Authorization::AppCapabilityAccess::
    AppCapabilityAccessStatus;

constexpr int kMacAddrLen = 6;
constexpr int kDefaultApFreq = -1;
constexpr int kUseEthernet = -2;
}  // namespace

WifiMedium::WifiMedium() { InitCapability(); }

bool WifiMedium::IsInterfaceValid() const { return wifi_interface_valid_; }

void WifiMedium::InitCapability() {
  wifi_capability_.supports_5_ghz = false;
  wifi_capability_.supports_6_ghz = false;
  wifi_interface_valid_ = false;

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

bool WifiMedium::GetWifiInformation() {
  WlanClient wlan_client;
  if (!wlan_client.Initialize()) {
    return false;
  }
  bool result = false;
  for (const auto& interface_info : wlan_client.GetInterfaceInfos()) {
    if (!interface_info.connected) {
      continue;
    }
    LOG(INFO) << "Found connected WiFi interface";
    wifi_information_.is_connected = true;

    DWORD result = 0;
    DWORD channel_size;
    ScopedWlanMemory<ULONG> channel;
    result = WlanQueryInterface(wlan_client.GetHandle(), &interface_info.guid,
                                wlan_intf_opcode_channel_number, nullptr,
                                &channel_size,
                                reinterpret_cast<PVOID*>(channel.OutParam()),
                                /*pWlanOpcodeValueType=*/nullptr);
    if (result != ERROR_SUCCESS || channel.get() == nullptr) {
      LOG(INFO) << "WlanQueryInterface channel error = " << result;
      return false;
    }
    wifi_information_.ap_frequency = WifiUtils::ConvertChannelToFrequencyMhz(
        *channel.get(), api::WifiBandType::kUnknown);
    LOG(INFO) << "Channel: " << (channel.get() == nullptr ? 0 : *channel.get())
              << "; ap_frequency: " << wifi_information_.ap_frequency;

    ScopedWlanMemory<WLAN_CONNECTION_ATTRIBUTES> connect_info;
    DWORD connect_info_size;
    result = WlanQueryInterface(
        wlan_client.GetHandle(), &interface_info.guid,
        wlan_intf_opcode_current_connection, nullptr, &connect_info_size,
        reinterpret_cast<PVOID*>(connect_info.OutParam()),
        /*pWlanOpcodeValueType=*/nullptr);
    if (result != ERROR_SUCCESS) {
      LOG(INFO) << "WlanQueryInterface current AP error = " << result;
      return false;
    }

    wifi_information_.ssid.assign(
        reinterpret_cast<const char*>(
            connect_info->wlanAssociationAttributes.dot11Ssid.ucSSID),
        connect_info->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
    LOG(INFO) << "wifi ssid is: " << wifi_information_.ssid
              << "; length is:" << wifi_information_.ssid.length();

    char str_tmp[kMacAddrLen];
    strncpy(str_tmp,
            reinterpret_cast<const char*>(
                connect_info->wlanAssociationAttributes.dot11Bssid),
            kMacAddrLen);
    wifi_information_.bssid = absl::StrFormat(
        "%02llx:%02llx:%02llx:%02llx:%02llx:%02llx", str_tmp[0], str_tmp[1],
        str_tmp[2], str_tmp[3], str_tmp[4], str_tmp[5]);
    LOG(INFO) << "wifi bssid is: " << wifi_information_.bssid;
    result = true;
  }
  return result;
}

// TODO(b/259414512): the return type should be optional.
api::WifiInformation& WifiMedium::GetInformation() {
  wifi_information_.is_connected = false;
  wifi_information_.ap_frequency = kDefaultApFreq;
  wifi_information_.ssid.clear();
  wifi_information_.bssid.clear();
  wifi_information_.ip_address_dot_decimal.clear();
  wifi_information_.ip_address_4_bytes.clear();

  AppCapability app_capability = AppCapability::Create(L"wiFiControl");
  AppCapabilityAccessStatus access_status = app_capability.CheckAccess();

  LOG(INFO) << "Wi-Fi access status:" << static_cast<int>(access_status);
  if (access_status != AppCapabilityAccessStatus::Allowed) {
    LOG(WARNING) << "Wi-Fi access is not granted";
    FillupEthernetParams();
    return wifi_information_;
  }

  if (!GetWifiInformation()) {
    FillupEthernetParams();
    return wifi_information_;
  }
  if (wifi_information_.is_connected) {
    wifi_information_.ip_address_dot_decimal = InternalGetWifiIpAddress();
    wifi_information_.ip_address_4_bytes = ipaddr_dotdecimal_to_4bytes_string(
        wifi_information_.ip_address_dot_decimal);
  } else {
    FillupEthernetParams();
  }

  return wifi_information_;
}

std::string WifiMedium::InternalGetWifiIpAddress() {
  try {
    auto host_names = NetworkInformation::GetHostNames();
    for (auto host_name : host_names) {
      if (host_name.IPInformation() != nullptr &&
          host_name.IPInformation().NetworkAdapter() != nullptr &&
          host_name.Type() == HostNameType::Ipv4) {
        std::string ipv4_s = winrt::to_string(host_name.ToString());
        LOG(INFO) << "Found IP: " << ipv4_s;

        auto profile = host_name.IPInformation()
                           .NetworkAdapter()
                           .GetConnectedProfileAsync()
                           .get();
        if (profile != nullptr && profile.IsWlanConnectionProfile()) {
          auto profile_details = profile.WlanConnectionProfileDetails();
          if (profile_details != nullptr &&
              wifi_information_.ssid ==
                  winrt::to_string(profile_details.GetConnectedSsid())) {
            LOG(INFO)
                << "SSID of this IP matches with this WiFi interface's SSID:"
                << wifi_information_.ssid << ", return this IP: " << ipv4_s;
            return ipv4_s;
          }
        }
      }
    }
    return {};
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {};
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return {};
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
    return {};
  }
}

std::string WifiMedium::InternalGetEthernetIpAddress() {
  try {
    auto host_names = NetworkInformation::GetHostNames();
    for (auto host_name : host_names) {
      if (host_name.IPInformation() != nullptr &&
          host_name.IPInformation().NetworkAdapter() != nullptr &&
          host_name.Type() == HostNameType::Ipv4) {
        std::string ipv4_s = winrt::to_string(host_name.ToString());
        if (host_name.IPInformation().NetworkAdapter().IanaInterfaceType() ==
            Constants::kInterfaceTypeEthernet) {
          LOG(INFO) << "Found IP: " << ipv4_s;
          return ipv4_s;
        }
      }
    }
    return {};
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
    return {};
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
    return {};
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
    return {};
  }
}

void WifiMedium::FillupEthernetParams() {
  wifi_information_.ip_address_dot_decimal = InternalGetEthernetIpAddress();
  if (wifi_information_.ip_address_dot_decimal.empty()) {
    LOG(INFO) << "No Etherent IP Addr found.";
    return;
  }
  wifi_information_.ip_address_4_bytes = ipaddr_dotdecimal_to_4bytes_string(
      wifi_information_.ip_address_dot_decimal);
  wifi_information_.ap_frequency = kUseEthernet;
}

}  // namespace windows
}  // namespace nearby
