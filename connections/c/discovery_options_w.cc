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

#include "connections/c/discovery_options_w.h"

#include <string>

namespace nearby::windows {

// Returns a copy and normalizes allowed mediums:
// (1) If is_out_of_band_connection is true, verifies that there is only one
//     medium allowed, defaulting to only Bluetooth if unspecified.
// (2) If no mediums are allowed, allow all mediums.
DiscoveryOptionsW DiscoveryOptionsW::CompatibleOptions() const {
  DiscoveryOptionsW result = *this;

  // Out-of-band connections initiate connections via an injected endpoint
  // rather than through the normal discovery flow. These types of connections
  // can only be injected via a single medium.
  if (is_out_of_band_connection) {
    int num_enabled = result.allowed.Count(true);

    // Default to allow only Bluetooth if no single medium is specified.
    if (num_enabled != 1) {
      result.allowed.SetAll(false);
      result.allowed.bluetooth = true;
    }
    return result;
  }

  // Normal connections (i.e., not out-of-band) connections can specify
  // multiple mediums. If none are specified, default to allowing all mediums.
  if (!allowed.Any(true)) result.allowed.SetAll(true);
  return result;
}

}  // namespace nearby::windows
