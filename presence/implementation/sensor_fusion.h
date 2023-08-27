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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SENSOR_FUSION_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SENSOR_FUSION_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "presence/device_motion.h"
#include "presence/presence_zone.h"

namespace nearby {
namespace presence {

enum class DataSource {
  kUnknown = 0,
  kBle = 1,
  kUwb = 2,
  kNanRtt = 4,
};

struct RangingMeasurement {
  // [0.0, 1.0], 1.0 is the max confidence.
  float confidence_level;
  float value;
};

struct RangingPosition {
  RangingMeasurement distance;
  std::optional<RangingMeasurement> azimuth;
  std::optional<RangingMeasurement> elevation;
  uint64_t elapsed_realtime_millis;
};

struct ZoneTransition {
  PresenceZone::DistanceBoundary::RangeType distance_range_type;
  float confidence_level;
};

struct RangingData {
  DataSource data_source;
  RangingPosition position;
  std::optional<ZoneTransition> zone_transition;
  std::vector<DeviceMotion> device_motions;
};

struct ZoneTransitionCallback {
  absl::AnyInvocable<void(
      uint64_t device_id,
      PresenceZone::DistanceBoundary::RangeType proximity_zone)>
      on_proximity_zone_changed =
          [](uint64_t device_id,
             PresenceZone::DistanceBoundary::RangeType proximity_zone) {};
  absl::AnyInvocable<void(uint64_t callback_id)> on_callback_id_generated =
      [](uint64_t callback_id) {};
};

class SensorFusion {
 public:
  virtual ~SensorFusion() = default;

  // Called when a device motion gesture is detected.
  typedef std::function<void(DeviceMotion::MotionType detected_device_motion)>
      DeviceMotionCallback;

  /**
   * Returns a list of data sources would be used by the sensor fusion if they
   * are available.
   * This is to control what kinds of sources the NP scan engine should use for
   * ranging. For instance, if both NAN RTT and UWB are supported, FPP may
   * decide NAN RTT isn't useful at a certain moment, so NP scan engine won't
   * try to request NAN RTT.
   *
   * @param elapsed_realtime_millis Elapsed timestamp since boot of the data
   * source query.
   * @param available_sources A bit mask of data sources that are available.
   */
  virtual std::vector<DataSource> GetDataSources(
      uint64_t elapsed_realtime_millis,
      const std::vector<DataSource>& available_sources) = 0;

  /**
   * Updates BLE scanned results to Sensor Fusion.
   *
   * @param device_id A unique device id of the peer device.
   * @param txPower Calibrated TX power of the scan result, {@code
   * std::nullopt} if the calibrated TX power is not available.
   * @param rssi Received signal strength indicator for the scan result.
   * @param elapsed_realtime_millis Elapsed timestamp since boot when the
   * scan result is discovered.
   */
  virtual absl::Status UpdateBleScanResult(
      uint64_t device_id, std::optional<int8_t> txPower, int rssi,
      uint64_t elapsed_realtime_millis) = 0;
  /**
   * Updates UWB ranging results to Sensor Fusion.
   *
   * @param device_id A unique device id of the peer device.
   * @param position UWB ranging result (distance and optionally angle)
   */
  virtual void UpdateUwbRangingResult(uint64_t device_id,
                                      RangingPosition position) = 0;

  /**
   * Adds callback for updates of proximity zone transitions.
   */
  virtual void RequestZoneTransitionUpdates(
      ZoneTransitionCallback callback) = 0;

  /**
   * Removes callback for updates of proximity zone transitions.
   */
  virtual void RemoveZoneTransitionUpdates(uint64_t callback_id) = 0;

  /**
   * Adds callback for updates of device motion events.
   */
  virtual void RequestDeviceMotionUpdates(DeviceMotionCallback callback) = 0;

  /**
   * Remove callback for updates of device motion events.
   */
  virtual void RemoveDeviceMotionUpdates(DeviceMotionCallback callback) = 0;

  /**
   * Returns the best ranging estimate to a given device. Returns {@code
   * std::nullopt} if the sensor fusion cannot produce a ranging estimate.
   *
   * @param device_id Id of the peer device.
   */
  virtual std::optional<RangingData> GetRangingData(uint64_t device_id) = 0;
};

}  // namespace presence
}  // namespace nearby
#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SENSOR_FUSION_H_

