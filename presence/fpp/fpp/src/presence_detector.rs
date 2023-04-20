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

use crate::fspl_converter::compute_distance_meters_at_high_tx_power;
use crate::fused_presence_utils::{
    BleScanResult, MeasurementConfidence, PresenceDataSource, ProximityEstimate, ProximityState,
    DEFAULT_CONSECUTIVE_SCANS_REQUIRED, DEFAULT_LONG_RANGE_DISTANCE_THRESHOLD_METERS,
    DEFAULT_REACH_DISTANCE_THRESHOLD_METERS, DEFAULT_SHORT_RANGE_DISTANCE_THRESHOLD_METERS,
    DEFAULT_TAP_DISTANCE_THRESHOLD_METERS,
};
use itertools::Itertools;
use std::collections::{HashMap, VecDeque};
use std::time::SystemTime;

const MAX_RSSI_FILTER_VALUE: i16 = 10;
const DEFAULT_ESTIMATED_DISTANCE_DATA_TTL_MILLIS: u128 = 4000;

// Static function for getting proximity state from threshold
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

pub struct PresenceDetector {
    last_range_update_time: u128,
    best_proximity_estimate_per_device: HashMap<u64, ProximityEstimate>,
    transition_history: VecDeque<ProximityState>,
}

impl PresenceDetector {
    pub fn new() -> Self {
        PresenceDetector {
            last_range_update_time: 0,
            best_proximity_estimate_per_device: HashMap::new(),
            transition_history: VecDeque::with_capacity(
                (DEFAULT_CONSECUTIVE_SCANS_REQUIRED + 1).into(),
            ),
        }
    }

    pub fn on_ble_scan_result(
        &mut self,
        ble_scan_result: BleScanResult,
    ) -> Option<ProximityEstimate> {
        let device_id = ble_scan_result.device_id;
        if ble_scan_result.rssi > MAX_RSSI_FILTER_VALUE {
            return self
                .best_proximity_estimate_per_device
                .get(&device_id)
                .copied();
        }
        let elapsed_real_time_millis = SystemTime::now().elapsed().unwrap().as_millis();
        if elapsed_real_time_millis - &self.last_range_update_time
            > DEFAULT_ESTIMATED_DISTANCE_DATA_TTL_MILLIS
        {
            self.transition_history.clear();
        }
        let mut tx_power: i16 = 0;
        if ble_scan_result.tx_power.present == true {
            tx_power = ble_scan_result.tx_power.value as i16;
        }
        let rssi = ble_scan_result.rssi + tx_power as i16;
        let distance_meters = compute_distance_meters_at_high_tx_power(rssi);
        let new_proximity_estimate = ProximityEstimate {
            device_id,
            distance_confidence: MeasurementConfidence::Low,
            distance: distance_meters,
            proximity_state: get_proximity_state_from_threshold(distance_meters),
            elapsed_real_time_millis,
            source: PresenceDataSource::Ble,
        };
        self.transition_history
            .push_front(new_proximity_estimate.proximity_state);
        self.transition_history
            .truncate(DEFAULT_CONSECUTIVE_SCANS_REQUIRED.into());
        if self.transition_history.iter().unique().count() == 1 {
            self.best_proximity_estimate_per_device
                .insert(device_id, new_proximity_estimate);
        }
        self.best_proximity_estimate_per_device
            .get(&device_id)
            .copied()
    }

    pub fn get_proximity_estimate(&mut self, device_id: u64) -> Option<ProximityEstimate> {
        self.best_proximity_estimate_per_device
            .get(&device_id)
            .copied()
    }
}
