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

#ifndef PLATFORM_API_WIFI_H_
#define PLATFORM_API_WIFI_H_

#include <cstdint>
#include <string>

namespace nearby::api {

// WiFi Band type.
enum class WifiBandType {
  kUnknown = 0,
  kBand24Ghz = 1,
  kBand5Ghz = 2,
  kBand6Ghz = 3,
  kBand60Ghz = 4,
};

// Native WiFi's capablity parameters
struct WifiCapability {
  bool supports_5_ghz = false;
  bool supports_6_ghz = false;
  bool support_wifi_direct = false;
};

// Native WiFi's information parameters
struct WifiInformation {
  // Is this WiFi interface connected to AP or not
  bool is_connected;
  // AP's SSID if connected
  std::string ssid;
  // WiFi LAN BSSID, in the form of a six-byte MAC address: XX:XX:XX:XX:XX:XX
  std::string bssid;
  // The frequency of the WiFi LAN AP(in MHz). Or -1 is not associated with an
  // AP over WiFi, -2 represents the active network uses an Ethernet transport.
  std::int32_t ap_frequency;
};

// Container of operations that can be performed over the WiFi medium.
class WifiMedium {
 public:
  virtual ~WifiMedium() {}

  virtual bool IsInterfaceValid() const = 0;

  virtual WifiCapability& GetCapability() = 0;

  virtual WifiInformation& GetInformation() = 0;
};

}  // namespace nearby::api

#endif  // PLATFORM_API_WIFI_H_
