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
#include <vector>

#include "absl/strings/string_view.h"

namespace nearby {
namespace api {

// Possible authentication types for a WiFi network.
enum class WifiAuthType {
  // WiFi Authentication type; either none (non-secured a.k.a. open) link, or
  // WPA PSK (WiFi Protected Access PreShared Key), or
  //   see https://en.wikipedia.org/wiki/Wi-Fi_Protected_Access
  // WEP (Wired Equivalent Privacy);
  //   see https://en.wikipedia.org/wiki/Wired_Equivalent_Privacy
  kUnknown = 0,
  kOpen = 1,
  kWpaPsk = 2,
  kWep = 3,
};

// Possible statuses of a device's connection to a WiFi network.
enum class WifiConnectionStatus {
  kUnknown = 0,
  kConnected = 1,
  kConnectionFailure = 2,
  kAuthFailure = 3,
};

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
  // The interface's IP address in the form of "xx.xx.xx.xx"
  std::string ip_address_dot_decimal;
  // IP address, in network byte order: the highest order byte of the address is
  // in byte[0].
  std::string ip_address_4_bytes;
};

// Represents a WiFi network found during a call to WifiMedium#scan().
class WifiScanResult {
 public:
  virtual ~WifiScanResult() = default;

  // Gets the SSID of this WiFi network.
  virtual std::string GetSsid() const = 0;
  // Gets the signal strength of this WiFi network in dBm.
  virtual std::int32_t GetSignalStrengthDbm() const = 0;
  // Gets the frequency band of this WiFi network in MHz.
  virtual std::int32_t GetFrequencyMhz() const = 0;
  // Gets the authentication type of this WiFi network.
  virtual WifiAuthType GetAuthType() const = 0;
};

// Container of operations that can be performed over the WiFi medium.
class WifiMedium {
 public:
  virtual ~WifiMedium() {}

  virtual bool IsInterfaceValid() const = 0;

  virtual WifiCapability& GetCapability() = 0;

  virtual WifiInformation& GetInformation() = 0;

  class ScanResultCallback {
   public:
    virtual ~ScanResultCallback() = default;

    virtual void OnScanResults(
        const std::vector<WifiScanResult>& scan_results) = 0;
  };

  // Does not take ownership of the passed-in scan_result_callback -- destroying
  // that is up to the caller.
  virtual bool Scan(const ScanResultCallback& scan_result_callback) = 0;

  // If 'password' is an empty string, none has been provided. Returns
  // WifiConnectionStatus::CONNECTED on success, or the appropriate failure code
  // otherwise.
  virtual WifiConnectionStatus ConnectToNetwork(absl::string_view ssid,
                                                absl::string_view password,
                                                WifiAuthType auth_type) = 0;

  // Blocks until it's certain of there being a connection to the internet, or
  // returns false if it fails to do so.
  //
  // How this method wants to verify said connection is totally up to it (so it
  // can feel free to ping whatever server, download whatever resource, etc.
  // that it needs to gain confidence that the internet is reachable hereon in).
  virtual bool VerifyInternetConnectivity() = 0;

  // Returns the local device's IP address in the IPv4 dotted-quad format.
  virtual std::string GetIpAddress() = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_WIFI_H_
