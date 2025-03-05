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
#ifndef CORE_DISCOVERY_OPTIONS_H_
#define CORE_DISCOVERY_OPTIONS_H_

#include <cstdint>
#include <optional>
#include <string>

#include "connections/options_base.h"

namespace nearby {
namespace connections {

struct BleOptions {
  // An alternative BLE service UUID16s for the Nearby service that is enabling
  // BLE scanning.
  std::optional<uint16_t> alternate_uuid;
};

// Discovery Options: used for service discovery.
// All fields are mutable, to make the type copy-assignable.
struct DiscoveryOptions : OptionsBase {
  // Returns a copy and normalizes allowed mediums:
  // (1) If is_out_of_band_connection is true, verifies that there is only one
  //     medium allowed, defaulting to only Bluetooth if unspecified.
  // (2) If no mediums are allowed, allow all mediums.
  DiscoveryOptions CompatibleOptions() const;
  bool auto_upgrade_bandwidth = true;
  bool enforce_topology_constraints;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  bool is_out_of_band_connection = false;
  // TODO(b/229927044): Replaces it as bool once Ble v1 is deprecated.
  std::string fast_advertisement_service_uuid;

  // If true, only low power mediums (like BLE) will be used for discovery.
  bool low_power = false;

  BleOptions ble_options;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_DISCOVERY_OPTIONS_H_
