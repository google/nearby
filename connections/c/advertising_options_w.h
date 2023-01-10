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
#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_ADVERTISING_OPTIONS_W_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_ADVERTISING_OPTIONS_W_H_

#include "connections/c/options_base_w.h"

namespace nearby::windows {

extern "C" {

// Advertising Options: used for Advertising.
// All fields are mutable, to make the type copy-assignable.
struct DLL_API AdvertisingOptionsW : public OptionsBaseW {
  bool auto_upgrade_bandwidth = true;
  bool enforce_topology_constraints;
  bool low_power;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  bool is_out_of_band_connection = false;
  const char* fast_advertisement_service_uuid;

  // The information about this device (eg. name, device type),
  // to appear on the remote device.
  // Defined by client/application.
  const char* device_info;

  // Returns a copy and normalizes allowed mediums:
  // (1) If is_out_of_band_connection is true, verifies that there is only one
  //     medium allowed, defaulting to only Bluetooth if unspecified.
  // (2) If no mediums are allowed, allow all mediums.
  AdvertisingOptionsW CompatibleOptions() const;
};

}  // extern "C"
}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_ADVERTISING_OPTIONS_W_H_
