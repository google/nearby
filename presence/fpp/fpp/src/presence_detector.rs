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

use crate::fused_presence_utils::{
    BleScanResult, ProximityEstimate, ProximityStateOptions, ProximityStateResult,
};
use crate::proximity_estimator::*;
use crate::proximity_state_detector::*;

const MAX_RSSI_FILTER_VALUE: i16 = 10;

pub struct PresenceDetector {
    proximity_estimator: ProximityEstimator,
    proximity_state_detector: ProximityStateDetector,
}

impl PresenceDetector {
    pub fn new() -> Self {
        PresenceDetector {
            proximity_estimator: ProximityEstimator::new(),
            proximity_state_detector: ProximityStateDetector::new(),
        }
    }

    pub fn on_ble_scan_result(
        &mut self,
        ble_scan_result: BleScanResult,
    ) -> Option<ProximityStateResult> {
        if &ble_scan_result.rssi > &MAX_RSSI_FILTER_VALUE {
            // faulty scan result received
            return None;
        }

        // Update proximity estimator with the new scan result
        self.proximity_estimator
            .on_ble_scan_result(&ble_scan_result);
        let device_id = ble_scan_result.device_id;
        let estimated_distance = self
            .proximity_estimator
            .get_proximity_estimate(device_id as u64);
        if estimated_distance != None {
            return Some(
                self.proximity_state_detector
                    .on_ranging_results_update(estimated_distance.unwrap()),
            );
        }
        None
    }

    pub fn configure_proximity_state_options(
        &mut self,
        proximity_state_options: ProximityStateOptions,
    ) {
        self.proximity_state_detector
            .configure_options(proximity_state_options);
    }

    pub fn get_proximity_estimate(&mut self, device_id: u64) -> Option<ProximityEstimate> {
        self.proximity_estimator.get_proximity_estimate(device_id)
    }
}
