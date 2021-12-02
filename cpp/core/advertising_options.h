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

#ifndef WINDOWS_START_ADVERTISING_OPTIONS_H_
#define WINDOWS_START_ADVERTISING_OPTIONS_H_

#include <stdint.h>

#include <memory>

#include "core/medium_selector.h"
#include "core/strategy.h"
#include "platform/base/core_config.h"

namespace location {
namespace nearby {
class ByteArray;
namespace connections {
using BooleanMediumSelector = MediumSelector<bool>;

enum class PowerLevel {
  kHighPower = 0,
  kLowPower = 1,
};

struct DLL_API AdvertisingOptions {
 public:
  connections::Strategy strategy;
  connections::BooleanMediumSelector allowed;

  bool auto_upgrade_bandwidth;
  bool enable_bluetooth_listening;
  bool enable_webrtc_listening;
  bool low_power;
  bool enforce_topology_constraints;

  //// Whether this is intended to be used in conjunction with InjectEndpoint().
  bool is_out_of_band_connection;
  const char *fast_advertisement_service_uuid;

  //// Returns a copy and normalizes allowed mediums:
  //// (1) If is_out_of_band_connection is true, verifies that there is only one
  ////     medium allowed, defaulting to only Bluetooth if unspecified.
  //// (2) If no mediums are allowed, allow all mediums.
  AdvertisingOptions CompatibleOptions() const;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // WINDOWS_START_ADVERTISING_OPTIONS_H_
