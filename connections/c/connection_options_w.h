// Copyright 2021-2022 Google LLC
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
#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_C_CONNECTION_OPTIONS_W_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_C_CONNECTION_OPTIONS_W_H_
#include <string>

#include "connections/c/dll_config.h"
#include "connections/c/medium_selector_w.h"
#include "connections/c/options_base_w.h"

namespace nearby::windows {

extern "C" {

#define MAX_MEDIUMS 6

// Connection Options: used for both Advertising and Discovery.
// All fields are mutable, to make the type copy-assignable.
struct DLL_API ConnectionOptionsW : public OptionsBaseW {
  bool auto_upgrade_bandwidth = true;
  bool enforce_topology_constraints;
  bool low_power;

  // Whether this is intended to be used in conjunction with InjectEndpoint().
  bool is_out_of_band_connection = false;
  const char* remote_bluetooth_mac_address;
  const char* fast_advertisement_service_uuid;
  int keep_alive_interval_millis = 0;
  int keep_alive_timeout_millis = 0;

  void GetMediums(const MediumW*, size_t) const;

 private:
  MediumW* mediums_[MAX_MEDIUMS];
  size_t mediums_size;
};

}  // extern "C"
}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_C_CONNECTION_OPTIONS_W_H_
