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

const AMBIGUITY_METERS: f64 = 0.06;
pub const DEFAULT_TAP_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 0.02;
pub const DEFAULT_REACH_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 0.5;
pub const DEFAULT_SHORT_RANGE_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 1.2;
pub const DEFAULT_LONG_RANGE_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 3.0;
pub const DEFAULT_CONSECUTIVE_SCANS_REQUIRED: u8 = 2;

// Proximity state from device to another in terms of actionability
#[derive(Eq, Hash, Copy, Clone, PartialEq)]
#[repr(C)]
pub enum ProximityState {
    Unknown,
    Tap,
    Reach,
    ShortRange,
    LongRange,
    Far,
}

// Represents the confidence levels for a given measurement
#[derive(Copy, Clone, PartialEq)]
#[repr(C)]
pub enum MeasurementConfidence {
    Low,
    Medium,
    High,
    Unknown,
}

// Data sources that are used to track presence
#[derive(Copy, Clone, PartialEq)]
#[repr(C)]
pub enum PresenceDataSource {
    Ble,
    Uwb,
    Nan,
    Unknown,
}

// A PII-stripped subset of Bluetooth scan result
#[repr(C)]
pub struct BleScanResult {
    pub device_id: u64,
    pub tx_power: COption<i16>,
    pub rssi: i16,
    pub elapsed_real_time: u64,
}

#[derive(Copy, Clone, PartialEq)]
#[repr(C)]
pub struct ProximityEstimate {
    pub device_id: u64,
    pub distance: f64,
    pub distance_confidence: MeasurementConfidence,
    pub elapsed_real_time_millis: u128,
    pub proximity_state: ProximityState,
    pub source: PresenceDataSource,
}

#[repr(C)]
pub struct COption<T> {
    pub value: *const T,
    pub present: bool,
}
