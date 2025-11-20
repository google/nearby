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

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "proto/connections_enums.pb.h"

namespace nearby {

struct ServiceAddress {
  // IP address in MSB-first order.
  // IPv4 address is 4 bytes, and IPv6 address is 16 bytes.
  std::vector<char> address;
  uint16_t port;
};

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

  const std::vector<ServiceAddress>& GetAddressCandidates() const {
    return address_candidates_;
  }
  void SetAddressCandidates(std::vector<ServiceAddress> address_candidates) {
    address_candidates_ = std::move(address_candidates);
  }

  // Gets the Frequency
  int GetFrequency() const { return frequency_; }
  // Set frequency_
  void SetFrequency(int frequency) { frequency_ = frequency; }

 private:
  std::string ssid_;
  std::string password_;
  int frequency_ = -1;
  std::vector<ServiceAddress> address_candidates_;
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

  // Get/Set Service Name.
  std::string GetServiceName() const { return service_name_; }
  void SetServiceName(const std::string& service_name) {
    service_name_ = service_name;
  }

  // Get/Set Pin.
  std::string GetPin() const { return pin_; }
  void SetPin(const std::string& pin) { pin_ = pin; }

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
  // Set the Frequency
  void SetFrequency(int frequency) { frequency_ = frequency; }

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
  // There are 2 types of WifiDirectAuthType.
  // 1. Without Service Discovery: the credentials are ssid/password.
  // 2. With Service Discovery: the credentials are service_name/pin.
  // Android supports type 1 and 2 in the future, but Windows only supports the
  // second type.
  std::string ssid_;
  std::string password_;
  std::string service_name_;
  std::string pin_;
  std::string ip_address_;
  std::string gateway_ = "0.0.0.0";
  int port_ = 0;
  int frequency_ = -1;
  location::nearby::proto::connections::ConnectionBand band_;
  location::nearby::proto::connections::ConnectionTechnology technology_;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_WIFI_CREDENTIAL_H_
