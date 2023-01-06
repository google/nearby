// Copyright 2020 Google LLC
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

#ifndef PLATFORM_BASE_WIFI_CREDENTIAL_H_
#define PLATFORM_BASE_WIFI_CREDENTIAL_H_

#include <string>

#include "absl/strings/str_format.h"
#include "internal/platform/prng.h"
#include "proto/connections_enums.pb.h"

namespace nearby {

// Credentials for the currently-hosted Wifi hotspot (if any)
// Class HotspotCredentials is copyable & movable
class HotspotCredentials {
 public:
  HotspotCredentials() = default;
  HotspotCredentials(const HotspotCredentials&) = default;
  HotspotCredentials& operator=(const HotspotCredentials&) = default;
  HotspotCredentials(HotspotCredentials&&) = default;
  HotspotCredentials& operator=(HotspotCredentials&&) = default;
  ~HotspotCredentials() = default;

  // Returns the SSID (Service Set Identifier) which the hotspot will
  // periodically beacon. STA can start a wifi scan for this SSID to find the
  // hotspot
  std::string GetSSID() const { return ssid_; }
  void SetSSID(const std::string& ssid) { ssid_ = ssid; }

  std::string GetPassword() const { return password_; }
  void SetPassword(const std::string& password) { password_ = password; }

  // Gets IP Address, which is in byte sequence, in network order. For example,
  // for "192.168.1.1", it'll be byte(129)+byte(168)+byte(1)+byte(1). Now only
  // ipv4 is supported.
  std::string GetIPAddress() const { return ip_address_; }
  void SetIPAddress(const std::string& ip_address) { ip_address_ = ip_address; }

  std::string GetGateway() const { return gateway_; }
  void SetGateway(const std::string& gateway) { gateway_ = gateway; }

  // Gets the Port number
  int GetPort() const { return port_; }
  // Set port_
  void SetPort(const int port) { port_ = port; }

  // Gets the Frequency
  int GetFrequency() const { return frequency_; }

  // Gets the Band
  location::nearby::proto::connections::ConnectionBand GetBand() const {
    return band_;
  }

  // Gets the Technology
  location::nearby::proto::connections::ConnectionTechnology GetTechnology()
      const {
    return technology_;
  }

 private:
  std::string ssid_;
  std::string password_;
  std::string ip_address_;
  std::string gateway_ = "0.0.0.0";
  int port_ = 0;
  int frequency_ = -1;
  location::nearby::proto::connections::ConnectionBand band_;
  location::nearby::proto::connections::ConnectionTechnology technology_;
};

// Credentials for the currently-hosted WifiDirect GO (if any)
// Class WifiDirectCredentials is copyable & movable
class WifiDirectCredentials {
 public:
  WifiDirectCredentials() = default;
  WifiDirectCredentials(const WifiDirectCredentials&) = default;
  WifiDirectCredentials& operator=(const WifiDirectCredentials&) = default;
  WifiDirectCredentials(WifiDirectCredentials&&) = default;
  WifiDirectCredentials& operator=(WifiDirectCredentials&&) = default;
  ~WifiDirectCredentials() = default;

  // Returns the SSID (Service Set Identifier) which the WifiDirect GO will
  // periodically beacon. STA can start a wifi scan for this SSID to find the
  // wifidirect go
  std::string GetSSID() const { return ssid_; }
  void SetSSID(const std::string& ssid) { ssid_ = ssid; }

  std::string GetPassword() const { return password_; }
  void SetPassword(const std::string& password) { password_ = password; }

  // Gets IP Address, which is in byte sequence, in network order. For example,
  // for "192.168.1.1", it'll be byte(129)+byte(168)+byte(1)+byte(1). Now only
  // ipv4 is supported.
  std::string GetIPAddress() const { return ip_address_; }
  void SetIPAddress(const std::string& ip_address) { ip_address_ = ip_address; }

  std::string GetGateway() const { return gateway_; }
  void SetGateway(const std::string& gateway) { gateway_ = gateway; }

  // Gets the Port number
  int GetPort() const { return port_; }
  // Set port_
  void SetPort(const int port) { port_ = port; }

  // Gets the Frequency
  int GetFrequency() const { return frequency_; }

  // Gets the Band
  location::nearby::proto::connections::ConnectionBand GetBand() const {
    return band_;
  }

  // Gets the Technology
  location::nearby::proto::connections::ConnectionTechnology GetTechnology()
      const {
    return technology_;
  }

 private:
  std::string ssid_;
  std::string password_;
  std::string ip_address_;
  std::string gateway_ = "0.0.0.0";
  int port_ = 0;
  int frequency_ = -1;
  location::nearby::proto::connections::ConnectionBand band_;
  location::nearby::proto::connections::ConnectionTechnology technology_;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_WIFI_CREDENTIAL_H_
