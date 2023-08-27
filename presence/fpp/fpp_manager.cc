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

#include "presence/fpp/fpp_manager.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "internal/platform/logging.h"
#include "presence/fpp/fpp_c_ffi/include/presence_detector.h"
#include "presence/implementation/sensor_fusion.h"
#include "presence/presence_zone.h"

namespace nearby {
namespace presence {

namespace {
// See
// https://source.corp.google.com/piper///depot/google3/third_party/nearby/presence/fpp/fpp_c_ffi/src/lib.rs;l=49
// for constants definition
constexpr int kSuccess = 1;
constexpr int kNoComputedProximityEstimate = 2;
constexpr int kInvalidPresenceDetectorHandleError = 101;
constexpr int kNullOutputParameterError = 102;

// Converts optional tx power to the rust api compatible equivalent
MaybeTxPower ConvertTxPower(std::optional<int8_t> txPower) {
  if (txPower.has_value()) {
    return {MaybeTxPower::Tag::Valid, {txPower.value()}};
  }
  return {MaybeTxPower::Tag::Invalid, {}};
}

// Converts FPP ProximityState struct to NP RangeType struct
PresenceZone::DistanceBoundary::RangeType ConvertProximityStateToRangeType(
    ProximityState proximity_state) {
  switch (proximity_state) {
    case ProximityState::Tap:
      return PresenceZone::DistanceBoundary::RangeType::kWithinTap;
    case ProximityState::Reach:
      return PresenceZone::DistanceBoundary::RangeType::kWithinReach;
    case ProximityState::ShortRange:
    case ProximityState::LongRange:
    case ProximityState::Far:
      return PresenceZone::DistanceBoundary::RangeType::kFar;
    case ProximityState::Unknown:
    default:
      NEARBY_LOGS(WARNING) << "Proximity state is unknown";
      return PresenceZone::DistanceBoundary::RangeType::kRangeUnknown;
  }
}
}  // namespace

absl::Status FppManager::UpdateBleScanResult(uint64_t device_id,
                                             std::optional<int8_t> txPower,
                                             int rssi,
                                             uint64_t elapsed_realtime_millis) {
  if (zone_transition_callbacks_.empty()) {
    return absl::InternalError("No callback registered");
  }
  BleScanResult ble_scan_result = {device_id, ConvertTxPower(txPower), rssi,
                                   elapsed_realtime_millis};
  ProximityEstimate default_proximity_estimate =
      ProximityEstimate{device_id,
                        /*distanceMeters=*/0.0,
                        MeasurementConfidence::Unknown,
                        /*elapsedRealtime=*/0,
                        ProximityState::Unknown,
                        PresenceDataSource::Ble};
  ProximityEstimate old_proximity_estimate =
      current_proximity_estimates_.contains(device_id)
          ? current_proximity_estimates_[device_id]
          : default_proximity_estimate;
  ProximityEstimate new_proximity_estimate = default_proximity_estimate;
  int status_code = update_ble_scan_result(
      presence_detector_handle_, ble_scan_result, &new_proximity_estimate);
  if (status_code == kNoComputedProximityEstimate) {
    NEARBY_LOGS(INFO) << "Insufficient number of scan results available to "
                         "compute proximity state";
    return absl::OkStatus();
  }
  if (status_code == kSuccess) {
    current_proximity_estimates_[device_id] = new_proximity_estimate;
    CheckPresenceZoneChanged(device_id, old_proximity_estimate,
                             new_proximity_estimate);
    return absl::OkStatus();
  }
  NEARBY_LOGS(WARNING)
      << "Could not successfully update FPP with new scan result: Error code="
      << status_code;
  return absl::InternalError(GetStatusStringFromCode(status_code));
}

void FppManager::RegisterZoneTransitionListener(
    uint64_t callback_id, ZoneTransitionCallback callback) {
  zone_transition_callbacks_[callback_id] = std::move(callback);
}

void FppManager::UnregisterZoneTransitionListener(uint64_t callback_id) {
  zone_transition_callbacks_.erase(callback_id);
}

void FppManager::ResetProximityStateData() {
  current_proximity_estimates_.clear();
}

std::optional<RangingData> FppManager::GetRangingData(uint64_t device_id) {
  return ConvertProximityEstimateToRangingData(
      current_proximity_estimates_[device_id]);
}

// Converts FPP ProximityEstimate struct to NP RangingData struct
RangingData FppManager::ConvertProximityEstimateToRangingData(
    ProximityEstimate estimate) {
  RangingMeasurement ranging_measurement = {
      /*confidenceLevel=*/0.0, static_cast<float>(estimate.distance_meters)};
  RangingPosition ranging_position = {
      ranging_measurement, /*azimuth=*/std::nullopt,
      /*elevation=*/std::nullopt, estimate.elapsed_real_time_millis};
  ZoneTransition zone_transition = {
      ConvertProximityStateToRangeType(estimate.proximity_state),
      /*confidenceLevel=*/0.0};
  return {DataSource::kBle, ranging_position, zone_transition,
          std::vector<DeviceMotion>()};
}

void FppManager::CheckPresenceZoneChanged(uint64_t device_id,
                                          ProximityEstimate old_estimate,
                                          ProximityEstimate new_estimate) {
  if (old_estimate.proximity_state != new_estimate.proximity_state) {
    NEARBY_LOG(WARNING,
               "Updating zone transition callbacks with new zone. Zone=%p",
               new_estimate.proximity_state);
    for (auto& pair : zone_transition_callbacks_) {
      pair.second.on_proximity_zone_changed(
          device_id,
          ConvertProximityStateToRangeType(new_estimate.proximity_state));
    }
  }
}

std::string FppManager::GetStatusStringFromCode(int status_code) {
  switch (status_code) {
    case kInvalidPresenceDetectorHandleError:
      return "INVALID_PRESENCE_DETECTOR_HANDLE";
    case kNullOutputParameterError:
      return "NULL_OUTPUT_PARAMETER";
    default:
      NEARBY_LOGS(WARNING) << "Error code is unknown";
      return "UNKNOWN_ERROR";
  }
}

}  // namespace presence
}  // namespace nearby
