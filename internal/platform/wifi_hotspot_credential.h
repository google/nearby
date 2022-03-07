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

#ifndef PLATFORM_BASE_WIFI_HOTSPOT_CREDENTIAL_H_
#define PLATFORM_BASE_WIFI_HOTSPOT_CREDENTIAL_H_

#include "absl/strings/str_format.h"
#include "internal/platform/prng.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {

// Credentials for the currently-hosted Wifi hotspot (if any)
class HotspotCredentials {
 public:
  HotspotCredentials() = default;
  HotspotCredentials(const HotspotCredentials&) = default;
  HotspotCredentials& operator=(const HotspotCredentials&) = default;
  HotspotCredentials(HotspotCredentials&&) = default;
  HotspotCredentials& operator=(HotspotCredentials&&) = default;
  ~HotspotCredentials() = default;

  // Get SSID
  std::string GetSSID() const { return ssid_; }
  // Set SSID
  void SetSSID(const std::string& ssid) { ssid_ = ssid; }

  // Get password_
  std::string GetPassword() const { return password_; }
  // Set password_
  void SetPassword(const std::string& password) { password_ = password; }

  // Gets IP Address, which is in byte sequence, in network order.
  std::string GetIPAddress() const { return ip_address_; }
  // Set ip_address_
  void SetIPAddress(const std::string& ip_address) { ip_address_ = ip_address; }

  // Gets Gateway
  std::string GetGateway() const { return gateway_; }
  // Set gateway_
  void SetGateway(const std::string& gateway) { gateway_ = gateway; }

  // Gets the Port number
  int GetPort() const { return port_; }
  // Set port_
  void SetPort(const int port) { port_ = port; }

  // Gets the Frequency
  int GetFrequency() const { return frequency_; }

  // Gets the Band
  proto::connections::ConnectionBand GetBand() const { return band_; }

  // Gets the Technology
  proto::connections::ConnectionTechnology GetTechnology() const {
    return technology_;
  }

 private:
  std::string ssid_ = "Nearby-" + std::to_string(Prng().NextUint32());
  std::string password_ = absl::StrFormat("%08x", Prng().NextUint32());
  std::string ip_address_;
  std::string gateway_ = {"0.0.0.0"};
  int port_ = 0;
  int frequency_ = -1;
  proto::connections::ConnectionBand band_;
  proto::connections::ConnectionTechnology technology_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_WIFI_HOTSPOT_CREDENTIAL_H_
