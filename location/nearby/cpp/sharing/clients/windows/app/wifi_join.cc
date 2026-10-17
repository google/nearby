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

#include "location/nearby/cpp/sharing/clients/windows/app/wifi_join.h"

// clang-format off
#include <windows.h>
#include <wlanapi.h>
// clang-format on

#include <string>
#include <string_view>

namespace nearby::sharing::windows_app {
namespace {

std::wstring Widen(std::string_view text) {
  if (text.empty()) return L"";
  int size = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                 static_cast<int>(text.size()), nullptr, 0);
  std::wstring wide(size, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      wide.data(), size);
  return wide;
}

std::wstring XmlEscape(const std::wstring& text) {
  std::wstring escaped;
  for (wchar_t c : text) {
    switch (c) {
      case L'&': escaped += L"&amp;"; break;
      case L'<': escaped += L"&lt;"; break;
      case L'>': escaped += L"&gt;"; break;
      case L'"': escaped += L"&quot;"; break;
      case L'\'': escaped += L"&apos;"; break;
      default: escaped.push_back(c);
    }
  }
  return escaped;
}

// WLAN_profile schema: authentication/encryption pairs per security type.
std::wstring ProfileXml(const WifiNetwork& network) {
  std::wstring ssid = XmlEscape(Widen(network.ssid));
  std::wstring auth = L"open";
  std::wstring encryption = L"none";
  std::wstring key_type;
  switch (network.security) {
    case WifiNetwork::Security::kWpaPsk:
      auth = L"WPA2PSK";
      encryption = L"AES";
      key_type = L"passPhrase";
      break;
    case WifiNetwork::Security::kSae:
      auth = L"WPA3SAE";
      encryption = L"AES";
      key_type = L"passPhrase";
      break;
    case WifiNetwork::Security::kWep:
      encryption = L"WEP";
      key_type = L"networkKey";
      break;
    default:
      break;
  }
  std::wstring xml =
      L"<?xml version=\"1.0\"?>"
      L"<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/"
      L"v1\"><name>" + ssid + L"</name><SSIDConfig><SSID><name>" + ssid +
      L"</name></SSID>" +
      (network.hidden ? L"<nonBroadcast>true</nonBroadcast>" : L"") +
      L"</SSIDConfig><connectionType>ESS</connectionType>"
      L"<connectionMode>auto</connectionMode><MSM><security><authEncryption>"
      L"<authentication>" + auth + L"</authentication><encryption>" +
      encryption + L"</encryption><useOneX>false</useOneX></authEncryption>";
  if (!key_type.empty()) {
    xml += L"<sharedKey><keyType>" + key_type +
           L"</keyType><protected>false</protected><keyMaterial>" +
           XmlEscape(Widen(network.password)) + L"</keyMaterial></sharedKey>";
  }
  xml += L"</security></MSM></WLANProfile>";
  return xml;
}

}  // namespace

std::wstring JoinWifiNetwork(const WifiNetwork& network) {
  HANDLE client = nullptr;
  DWORD version = 0;
  if (WlanOpenHandle(2, nullptr, &version, &client) != ERROR_SUCCESS) {
    return L"The WLAN service is not available.";
  }
  std::wstring error;
  WLAN_INTERFACE_INFO_LIST* interfaces = nullptr;
  if (WlanEnumInterfaces(client, nullptr, &interfaces) != ERROR_SUCCESS ||
      interfaces->dwNumberOfItems == 0) {
    error = L"No Wi-Fi adapter was found.";
  } else {
    const GUID& adapter = interfaces->InterfaceInfo[0].InterfaceGuid;
    std::wstring xml = ProfileXml(network);
    DWORD reason = 0;
    DWORD result = WlanSetProfile(client, &adapter, 0, xml.c_str(), nullptr,
                                  TRUE, nullptr, &reason);
    if (result != ERROR_SUCCESS) {
      wchar_t text[256] = L"";
      WlanReasonCodeToString(reason, 256, text, nullptr);
      error = L"Windows did not accept the network settings. ";
      error += text;
    } else {
      std::wstring profile = Widen(network.ssid);
      WLAN_CONNECTION_PARAMETERS parameters = {};
      parameters.wlanConnectionMode = wlan_connection_mode_profile;
      parameters.strProfile = profile.c_str();
      parameters.dot11BssType = dot11_BSS_type_infrastructure;
      if (WlanConnect(client, &adapter, &parameters, nullptr) !=
          ERROR_SUCCESS) {
        error = L"Saved the network, but could not connect to it now.";
      }
    }
  }
  if (interfaces != nullptr) WlanFreeMemory(interfaces);
  WlanCloseHandle(client, nullptr);
  return error;
}

}  // namespace nearby::sharing::windows_app
