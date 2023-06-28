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

pub(crate) const DEFAULT_TAP_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 0.02;
pub(crate) const DEFAULT_REACH_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 0.5;
pub(crate) const DEFAULT_SHORT_RANGE_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 1.2;
pub(crate) const DEFAULT_LONG_RANGE_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 3.0;
pub(crate) const DEFAULT_CONSECUTIVE_SCANS_REQUIRED: u8 = 2;
const AMBIGUITY_METERS: f64 = 0.06;

/// Proximity state from device to another in terms of actionability
#[derive(Eq, Hash, Copy, Clone, PartialEq, Debug)]
#[repr(C)]
pub enum ProximityState {
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
}

/// Represents the confidence levels for a given measurement
#[derive(Copy, Clone, PartialEq, Debug)]
#[repr(C)]
pub enum MeasurementConfidence {
    /// Measurement confidence is low, the default for BLE medium
    Low,
    /// Measurement confidence is medium
    Medium,
    /// Measurement confidence is High
    High,
    /// Measurement confidence is unknown
    Unknown,
}

/// Data sources that are used to track presence
#[derive(Copy, Clone, PartialEq, Debug)]
#[repr(C)]
pub enum PresenceDataSource {
    /// Data source for proximity estimate is BLE
    Ble,
    /// Data source for proximity estimate is UWB
    Uwb,
    /// Data source for proximity estimate is NAN
    Nan,
    /// Data source for proximity estimate is unknown
    Unknown,
}

/// A PII-stripped subset of Bluetooth scan result
#[repr(C)]
pub struct BleScanResult {
    /// Device ID of the nearby device
    pub device_id: u64,
    /// Transmitting power of signal
    pub tx_power: MaybeTxPower,
    /// RSSI value
    pub rssi: i32,
    /// Time scan result was obtained
    pub elapsed_real_time_millis: u64,
}

/// Enum representing an optional tx power value
#[repr(C)]
pub enum MaybeTxPower {
    /// Valid TX power with associated data value
    Valid(i32),
    /// Absent Tx Power
    Invalid,
}

/// Describes the most accurate and recent measurement for a given device
#[derive(Copy, Clone, PartialEq, Debug)]
#[repr(C)]
pub struct ProximityEstimate {
    /// Device ID of the nearby device
    pub device_id: u64,
    /// Distance to the nearby device in meters
    pub distance_meters: f64,
    /// Measurement confidence of the estimate
    pub distance_confidence: MeasurementConfidence,
    /// The time the proximity estimate was obtained (milliseconds since the
    /// program start time)
    pub elapsed_real_time_millis: u64,
    /// Proximity state zone of the nearby device
    pub proximity_state: ProximityState,
    /// Medium through which the proximity estimate was computed
    pub source: PresenceDataSource,
}
