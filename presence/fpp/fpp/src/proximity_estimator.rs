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

use std::collections::HashMap;
use std::time::SystemTime;

use crate::fspl_converter::*;
use crate::fused_presence_utils::{
    BleScanResult, MeasurementConfidence, PresenceDataSource, ProximityEstimate,
};

pub const DEFAULT_ESTIMATED_DISTANCE_DATA_TTL_MILLIS: u128 = 4000;

pub struct ProximityEstimator {
    estimated_distance_data_ttl_millis: u128,
    best_proximity_estimate_per_device: HashMap<u64, ProximityEstimate>,
}

impl ProximityEstimator {
    pub fn new() -> Self {
        ProximityEstimator {
            estimated_distance_data_ttl_millis: DEFAULT_ESTIMATED_DISTANCE_DATA_TTL_MILLIS,
            best_proximity_estimate_per_device: HashMap::new(),
        }
    }

    pub fn on_ble_scan_result(&mut self, ble_scan_result: &BleScanResult) {
        let device_id = ble_scan_result.device_id;
        let elapsed_real_time_millis = SystemTime::now().elapsed().unwrap().as_millis();
        let last_estimated_proximity_estimate =
            self.best_proximity_estimate_per_device.get(&device_id);
        if last_estimated_proximity_estimate != None
            && last_estimated_proximity_estimate.unwrap().source != PresenceDataSource::Ble
            && (elapsed_real_time_millis
                - last_estimated_proximity_estimate
                    .unwrap()
                    .elapsed_real_time_millis as u128)
                < self.estimated_distance_data_ttl_millis as u128
        {
            // Skip if last estimation is still fresh and is from a more precise data source (uwb/nan)
            return;
        }
        let mut tx_power: i16 = 0;
        if ble_scan_result.tx_power.present == true {
            tx_power = ble_scan_result.tx_power.value as i16;
        }
        let rssi = ble_scan_result.rssi + tx_power as i16;
        let best_proximity_estimate: ProximityEstimate = ProximityEstimate {
            device_id,
            distance_confidence: MeasurementConfidence::Low,
            distance: compute_distance_meters_at_high_tx_power(rssi),
            elapsed_real_time_millis,
            source: PresenceDataSource::Ble,
        };
        self.best_proximity_estimate_per_device
            .insert(device_id, best_proximity_estimate);
    }

    pub fn set_estimated_distances_ttl_millis(&mut self, ttl_millis: u128) {
        self.estimated_distance_data_ttl_millis = ttl_millis;
    }

    pub fn get_proximity_estimate(&mut self, device_id: u64) -> Option<ProximityEstimate> {
        let elapsed_real_time_millis = SystemTime::now().elapsed().unwrap().as_millis();

        // Exit early if no recorded proximity estimates
        if self.best_proximity_estimate_per_device.get(&device_id) == None {
            return None;
        }

        let elapsed_time_since_range_result = elapsed_real_time_millis
            - self
                .best_proximity_estimate_per_device
                .get(&device_id)
                .unwrap()
                .elapsed_real_time_millis;

        // If stale, remove entry + return nothing
        if elapsed_time_since_range_result >= self.estimated_distance_data_ttl_millis {
            self.best_proximity_estimate_per_device.remove(&device_id);
            return None;
        }

        return self
            .best_proximity_estimate_per_device
            .get(&device_id)
            .copied();
    }
}
