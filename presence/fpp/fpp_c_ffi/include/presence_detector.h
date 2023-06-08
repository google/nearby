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

#ifndef PRESENCE_DETECTOR_H_
#define PRESENCE_DETECTOR_H_

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

// Represents the confidence levels for a given measurement
enum class MeasurementConfidence {
  /// Measurement confidence is low, the default for BLE medium
  Low,
  /// Measurement confidence is medium
  Medium,
  /// Measurement confidence is High
  High,
  /// Measurement confidence is unknown
  Unknown,
};

/// Data sources that are used to track presence
enum class PresenceDataSource {
  /// Data source for proximity estimate is BLE
  Ble,
  /// Data source for proximity estimate is UWB
  Uwb,
  /// Data source for proximity estimate is NAN
  Nan,
  /// Data source for proximity estimate is unknown
  Unknown,
};

/// Proximity state from device to another in terms of actionability
enum class ProximityState {
  /// Unknown proximity state
  Unknown,
  /// The device is within a tap zone (<0.02m)
  Tap,
  /// The device is within a reach zone (<0.5m)
  Reach,
  /// The device is within a short range zone (<1.2m)
  ShortRange,
  /// The device is within a long range zone (<3.0m)
  LongRange,
  /// The device is at a far range
  Far,
};

/// Wraps the handle ID to an underlying PresenceDetector object
struct PresenceDetectorHandle {
  uint64_t handle;
};

/// Enum representing an optional tx power value
struct MaybeTxPower {
  enum class Tag {
    /// Valid TX power with associated data value
    Valid,
    /// Absent Tx Power
    Invalid,
  };

  struct Valid_Body {
    int32_t _0;
  };

  Tag tag;
  union {
    Valid_Body valid;
  };
};

/// A PII-stripped subset of Bluetooth scan result
struct BleScanResult {
  /// Device ID of the nearby device
  uint64_t device_id;
  /// Transmitting power of signal
  MaybeTxPower tx_power;
  /// RSSI value
  int32_t rssi;
  /// Time scan result was obtained
  uint64_t elapsed_real_time_millis;
};

/// Describes the most accurate and recent measurement for a given device
struct ProximityEstimate {
  /// Device ID of the nearby device
  uint64_t device_id;
  /// Distance to the nearby device in meters
  double distance_meters;
  /// Measurement confidence of the estimate
  MeasurementConfidence distance_confidence;
  /// The time the proximity estimate was obtained
  uint64_t elapsed_real_time_millis;
  /// Proximity state zone of the nearby device
  ProximityState proximity_state;
  /// Medium through which the proximity estimate was computed
  PresenceDataSource source;
};

extern "C" {

/// Creates a new presence detector object and returns the handle for the new
/// object
PresenceDetectorHandle presence_detector_create();

/// Updates PresenceDetector with a new scan result and returns an error code
/// if unsuccessful
///
/// # Safety
///
/// Ensure that the output parameter refers to an initialized instance
int32_t update_ble_scan_result(PresenceDetectorHandle presence_detector_handle,
                               BleScanResult ble_scan_result,
                               ProximityEstimate *proximity_estimate);

/// Gets the current proximity estimate for a given device ID
///
/// # Safety
///
/// Ensure that the output parameter refers to an initialized instance
int32_t get_proximity_estimate(PresenceDetectorHandle presence_detector_handle,
                               uint64_t device_id,
                               ProximityEstimate *proximity_estimate);

/// De-allocates memory for a presence detector object
int presence_detector_free(PresenceDetectorHandle presence_detector_handle);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // PRESENCE_DETECTOR_H_
