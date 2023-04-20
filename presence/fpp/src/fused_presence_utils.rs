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

use lazy_static::lazy_static;
use std::collections::HashMap;

const AMBIGUITY_METERS: f64 = 0.06;
pub const DEFAULT_TAP_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 0.02;
pub const DEFAULT_REACH_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 0.5;
pub const DEFAULT_SHORT_RANGE_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 1.2;
pub const DEFAULT_LONG_RANGE_DISTANCE_THRESHOLD_METERS: f64 = AMBIGUITY_METERS + 3.0;
pub const DEFAULT_CONSECUTIVE_SCANS_REQUIRED: u8 = 2;
pub const DEFAULT_HYSTERESIS_PERCENTAGE: f64 = 0.2;
pub const DEFAULT_HYSTERESIS_TAP_EXTRA_DEBOUNCE_METERS: f64 = 0.2;

pub const DEFAULT_PROXIMITY_STATE_OPTIONS: ProximityStateOptions = ProximityStateOptions {
    tap_threshold_meters: DEFAULT_TAP_DISTANCE_THRESHOLD_METERS,
    reach_threshold_meters: DEFAULT_REACH_DISTANCE_THRESHOLD_METERS,
    short_range_threshold_meters: DEFAULT_SHORT_RANGE_DISTANCE_THRESHOLD_METERS,
    long_range_threshold_meters: DEFAULT_LONG_RANGE_DISTANCE_THRESHOLD_METERS,
    hysteresis_percentage: DEFAULT_HYSTERESIS_PERCENTAGE,
    hysteresis_tap_extra_debounce_meters: DEFAULT_HYSTERESIS_TAP_EXTRA_DEBOUNCE_METERS,
    consecutive_scans_required: DEFAULT_CONSECUTIVE_SCANS_REQUIRED,
};

lazy_static! {
  pub static ref MAP_BLE_ADJUSTED_PROXIMITY_STATES: HashMap<ProximityState, ProximityState> = {
    let mut m = HashMap::new();
    // For BLE medium, reported granularity is reduced and all ranges are either TAP or LONG_RANGE
    m.insert(ProximityState::Tap, ProximityState::Tap);
    m.insert(ProximityState::Reach, ProximityState::Reach);
    m.insert(ProximityState::ShortRange, ProximityState::Far);
    m.insert(ProximityState::LongRange, ProximityState::Far);
    m.insert(ProximityState::Far, ProximityState::Far);
    m.insert(ProximityState::Unknown, ProximityState::Unknown);
    m
  };
}

// Proximity state from device to another in terms of actionability
#[derive(Eq, Hash, Copy, Clone, PartialEq)]
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
pub enum MeasurementConfidence {
    Low,
    Medium,
    High,
    Unknown,
}

// Data sources that are used to track presence
#[derive(Copy, Clone, PartialEq)]
pub enum PresenceDataSource {
    Ble,
    Uwb,
    Nan,
    Unknown,
}

// A PII-stripped subset of Bluetooth scan result
pub struct BleScanResult {
    pub device_id: u64,
    pub tx_power: Option<u8>,
    pub rssi: i16,
    pub elapsed_real_time: u64,
}

#[derive(Copy, Clone, PartialEq)]
pub struct ProximityEstimate {
    pub device_id: u64,
    pub distance: f64,
    pub distance_confidence: MeasurementConfidence,
    pub elapsed_real_time_millis: u128,
    pub source: PresenceDataSource,
}

#[derive(Copy, Clone, PartialEq)]
pub struct ProximityStateOptions {
    pub tap_threshold_meters: f64,
    pub reach_threshold_meters: f64,
    pub short_range_threshold_meters: f64,
    pub long_range_threshold_meters: f64,
    pub hysteresis_percentage: f64,
    pub hysteresis_tap_extra_debounce_meters: f64,
    pub consecutive_scans_required: u8,
}
