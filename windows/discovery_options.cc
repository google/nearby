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

#include "third_party/nearby/windows/discovery_options.h"

#include "core/options.h"
#include "core/strategy.h"
#include "platform/base/byte_array.h"
#include "platform/base/core_config.h"

namespace location {
namespace nearby {
namespace connections {
namespace windows {

DiscoveryOptions::DiscoveryOptions()
    : impl_(new connections::ConnectionOptions(),
            [](connections::ConnectionOptions *impl) { delete impl; }),
      strategy(impl_->strategy),
      allowed(impl_->allowed),
      auto_upgrade_bandwidth(impl_->auto_upgrade_bandwidth),
      enable_bluetooth_listening(impl_->enable_bluetooth_listening),
      enable_webrtc_listening(impl_->enable_webrtc_listening),
      low_power(impl_->low_power),
      enforce_topology_constraints(impl_->enforce_topology_constraints),
      is_out_of_band_connection(impl_->is_out_of_band_connection),
      remote_bluetooth_mac_address(impl_->remote_bluetooth_mac_address),
      fast_advertisement_service_uuid(
          impl_->fast_advertisement_service_uuid.c_str()),
      keep_alive_interval_millis(impl_->keep_alive_interval_millis),
      keep_alive_timeout_millis(impl_->keep_alive_timeout_millis) {}

DiscoveryOptions::operator connections::ConnectionOptions() const {
  return *impl_.get();
}

// Verify if  ConnectionOptions is in a not-initialized (Empty) state.
bool DiscoveryOptions::Empty() const { return impl_->Empty(); }

// Bring  ConnectionOptions to a not-initialized (Empty) state.
void DiscoveryOptions::Clear() {}

// Returns a copy and normalizes allowed mediums:
// (1) If is_out_of_band_connection is true, verifies that there is only one
//     medium allowed, defaulting to only Bluetooth if unspecified.
// (2) If no mediums are allowed, allow all mediums.
connections::ConnectionOptions DiscoveryOptions::CompatibleOptions() const {
  return impl_->CompatibleOptions();
}

// This call follows the standard Microsoft calling pattern of calling first
// to get the size of the array. Caller then allocates memory for the array,
// and makes this call again to copy the array into the provided location.
void DiscoveryOptions::GetMediums(
    location::nearby::proto::connections::Medium *mediums,
    uint32_t *mediumsSize) {}

}  // namespace windows
}  // namespace connections
}  // namespace nearby
}  // namespace location
