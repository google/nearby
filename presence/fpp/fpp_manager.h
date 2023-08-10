// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_NEARBY_PRESENCE_FPP_FPP_MANAGER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_FPP_FPP_MANAGER_H_

#include <cstdint>
#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "presence/fpp/fpp_c_ffi/include/presence_detector.h"
#include "presence/implementation/sensor_fusion.h"
#include "presence/presence_zone.h"

namespace nearby {
namespace presence {

// Manages fused presence updates and serves as a sync -> async converter class
// between fpp and NP sensor fusion
class FppManager {
 public:
  using RangeType = PresenceZone::DistanceBoundary::RangeType;

  FppManager() { presence_detector_handle_ = presence_detector_create(); }
  ~FppManager() { presence_detector_free(presence_detector_handle_); }

  /** Updates FPP with new BLE scan results. Returns status code */
  absl::Status UpdateBleScanResult(uint64_t device_id,
                                   std::optional<int8_t> txPower, int rssi,
                                   uint64_t elapsed_realtime_millis);
  /**
   * Adds callback for updates of proximity zone transitions.
   */
  void RegisterZoneTransitionListener(uint64_t callback_id,
                                      ZoneTransitionCallback callback);

  /**
   * Unregister callback for updates of proximity zone transitions.
   */
  void UnregisterZoneTransitionListener(uint64_t callback_id);

  /**
   * Clears all proximity state data
   */
  void ResetProximityStateData();

  /*
   * Converts ProximityEstimate to a NP compatible struct
   */
  RangingData ConvertProximityEstimateToRangingData(ProximityEstimate estimate);

  /**
   * Gets the most recent ranging data for a given device
   */
  std::optional<RangingData> GetRangingData(uint64_t device_id);

  /*
   * Converts a status code to a string representation
   */
  std::string GetStatusStringFromCode(int status_code);

 private:
  void CheckPresenceZoneChanged(uint64_t device_id,
                                ProximityEstimate old_estimate,
                                ProximityEstimate new_estimate);
  absl::flat_hash_map<uint64_t, ProximityEstimate> current_proximity_estimates_;
  absl::flat_hash_map<uint64_t, ZoneTransitionCallback>
      zone_transition_callbacks_;
  PresenceDetectorHandle presence_detector_handle_;
};
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_FPP_FPP_MANAGER_H_
