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

use std::collections::{HashMap, VecDeque};
use std::time::{Instant, SystemTime};

use itertools::Itertools;

use crate::fspl_converter::compute_distance_meters_at_high_tx_power;
use crate::fused_presence_utils::{
    BleScanResult, MaybeTxPower, MeasurementConfidence, PresenceDataSource, ProximityEstimate,
    ProximityState, DEFAULT_CONSECUTIVE_SCANS_REQUIRED,
    DEFAULT_LONG_RANGE_DISTANCE_THRESHOLD_METERS, DEFAULT_REACH_DISTANCE_THRESHOLD_METERS,
    DEFAULT_SHORT_RANGE_DISTANCE_THRESHOLD_METERS, DEFAULT_TAP_DISTANCE_THRESHOLD_METERS,
};

const MAX_RSSI_FILTER_VALUE: i32 = 10;
const DEFAULT_ESTIMATED_DISTANCE_DATA_TTL_MILLIS: u128 = 4000;

/// Static function for getting proximity state from threshold
fn get_proximity_state_from_threshold(distance_meters: f64) -> ProximityState {
    if distance_meters <= DEFAULT_TAP_DISTANCE_THRESHOLD_METERS {
        return ProximityState::Tap;
    }
    if distance_meters <= DEFAULT_REACH_DISTANCE_THRESHOLD_METERS {
        return ProximityState::Reach;
    }
    if distance_meters <= DEFAULT_SHORT_RANGE_DISTANCE_THRESHOLD_METERS {
        return ProximityState::ShortRange;
    }
    if distance_meters <= DEFAULT_LONG_RANGE_DISTANCE_THRESHOLD_METERS {
        return ProximityState::LongRange;
    }
    ProximityState::Far
}

/// Tracks and computes proximity/presence state events.
pub struct PresenceDetector {
    start_time: Instant,
    last_range_update_time: RangingUpdateTime,
    best_proximity_estimate_per_device: HashMap<u64, ProximityEstimate>,
    transition_history: VecDeque<ProximityState>,
}

struct RangingUpdateTime(u128);

impl RangingUpdateTime {
    pub fn is_expired(&self) -> bool {
        let elapsed_real_time_millis = Instant::now().elapsed().as_millis();
        elapsed_real_time_millis - self.0 > DEFAULT_ESTIMATED_DISTANCE_DATA_TTL_MILLIS
    }

    pub fn update(&mut self, start_time: Instant) {
        self.0 = Instant::now().duration_since(start_time).as_millis();
    }
}

impl PresenceDetector {
    /// Creates a new instance of presence detector
    pub fn new() -> Self {
        PresenceDetector {
            start_time: Instant::now(),
            last_range_update_time: RangingUpdateTime(0),
            best_proximity_estimate_per_device: HashMap::new(),
            transition_history: VecDeque::with_capacity(
                (DEFAULT_CONSECUTIVE_SCANS_REQUIRED + 1).into(),
            ),
        }
    }

    /// Updates the presence detector with a new scan result and returns the
    /// current proximity estimate
    pub fn on_ble_scan_result(
        &mut self,
        ble_scan_result: BleScanResult,
    ) -> Option<ProximityEstimate> {
        let device_id = ble_scan_result.device_id;
        if ble_scan_result.rssi > MAX_RSSI_FILTER_VALUE {
            return self.best_proximity_estimate_per_device.get(&device_id).copied();
        }
        if self.last_range_update_time.is_expired() {
            self.transition_history.clear();
        }
        let mut tx_power: i32 = 0;
        if let MaybeTxPower::Valid(some_tx_power) = ble_scan_result.tx_power {
            tx_power = some_tx_power;
        }
        let rssi = ble_scan_result.rssi + tx_power;
        let distance_meters = compute_distance_meters_at_high_tx_power(rssi);
        let new_proximity_estimate = ProximityEstimate {
            device_id,
            distance_confidence: MeasurementConfidence::Low,
            distance_meters,
            proximity_state: get_proximity_state_from_threshold(distance_meters),
            elapsed_real_time_millis: Instant::now().duration_since(self.start_time).as_millis()
                as u64,
            source: PresenceDataSource::Ble,
        };
        self.transition_history.push_front(new_proximity_estimate.proximity_state);
        self.transition_history.truncate(DEFAULT_CONSECUTIVE_SCANS_REQUIRED.into());
        if self.transition_history.iter().unique().count() == 1
            && self.transition_history.len() == DEFAULT_CONSECUTIVE_SCANS_REQUIRED.into()
        {
            self.best_proximity_estimate_per_device.insert(device_id, new_proximity_estimate);
            self.last_range_update_time.update(self.start_time);
        }
        self.best_proximity_estimate_per_device.get(&device_id).copied()
    }

    /// Returns the current proximity estimate for a given device
    pub fn get_proximity_estimate(&self, device_id: u64) -> Option<ProximityEstimate> {
        self.best_proximity_estimate_per_device.get(&device_id).copied()
    }
}

impl Default for PresenceDetector {
    fn default() -> Self {
        Self::new()
    }
}
