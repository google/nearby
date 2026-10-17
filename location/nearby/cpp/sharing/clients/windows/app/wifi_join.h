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
#ifndef LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_WIFI_JOIN_H_
#define LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_WIFI_JOIN_H_

#include <string>

namespace nearby::sharing::windows_app {

// A Wi-Fi network received from another device.
struct WifiNetwork {
  enum class Security { kUnknown, kOpen, kWpaPsk, kWep, kSae };
  std::string ssid;
  std::string password;
  Security security = Security::kUnknown;
  bool hidden = false;
};

// Saves the network as a WLAN profile on the first Wi-Fi interface and
// connects to it. Returns an empty string on success, else the reason.
std::wstring JoinWifiNetwork(const WifiNetwork& network);

}  // namespace nearby::sharing::windows_app

#endif  // LOCATION_NEARBY_CPP_SHARING_CLIENTS_WINDOWS_APP_WIFI_JOIN_H_
