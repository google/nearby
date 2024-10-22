// Copyright 2021 Google LLC
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
#ifndef CORE_CONNECTION_OPTIONS_H_
#define CORE_CONNECTION_OPTIONS_H_
#include <string>
#include <vector>

#include "connections/options_base.h"
#include "internal/platform/byte_array.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {

struct ConnectionInfo {
  std::string local_endpoint_id;
  ByteArray local_endpoint_info;
  std::int32_t nonce;
  bool supports_5_ghz = false;
  std::string bssid;
  std::int32_t ap_frequency = -1;
  std::string ip_address;
  std::vector<location::nearby::proto::connections::Medium> supported_mediums;
  std::int32_t keep_alive_interval_millis;
  std::int32_t keep_alive_timeout_millis;
};

// Connection Options: used for both Advertising and Discovery.
// All fields are mutable, to make the type copy-assignable.
struct ConnectionOptions : public OptionsBase {
  bool auto_upgrade_bandwidth = true;
  bool enforce_topology_constraints;
  bool low_power;
  bool enable_bluetooth_listening;
  bool enable_webrtc_listening;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  bool is_out_of_band_connection = false;
  ByteArray remote_bluetooth_mac_address;
  std::string fast_advertisement_service_uuid;
  int keep_alive_interval_millis = 0;
  int keep_alive_timeout_millis = 0;

  // If true, only use WiFi Hotspot for connection when Wifi LAN is not
  // connected.
  bool non_disruptive_hotspot_mode = false;

  std::vector<Medium> GetMediums() const;
  ConnectionInfo connection_info;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_CONNECTION_OPTIONS_H_
