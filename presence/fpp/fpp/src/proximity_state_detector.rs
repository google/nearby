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
use std::ops::Deref;
use std::time::SystemTime;

use crate::device_proximity_data::DeviceProximityData;
use crate::fused_presence_utils::*;
use crate::proximity_estimator::*;

pub struct ProximityStateDetector {
    device_proximity_data_map: HashMap<u64, DeviceProximityData>,
    last_range_update_time: u128,
    proximity_state_options: ProximityStateOptions,
}

impl ProximityStateDetector {
    pub fn new() -> Self {
        ProximityStateDetector {
            device_proximity_data_map: HashMap::new(),
            last_range_update_time: 0,
            proximity_state_options: DEFAULT_PROXIMITY_STATE_OPTIONS,
        }
    }

    pub fn configure_options(&mut self, proximity_state_options: ProximityStateOptions) {
        self.proximity_state_options = proximity_state_options;
        for data in self.device_proximity_data_map.values_mut() {
            data.configure_options(proximity_state_options);
        }
    }

    pub fn on_ranging_results_update(
        &mut self,
        proximity_estimate: ProximityEstimate,
    ) -> ProximityStateResult {
        let current_time_millis = SystemTime::now().elapsed().unwrap().as_millis();
        if current_time_millis - &self.last_range_update_time
            > DEFAULT_ESTIMATED_DISTANCE_DATA_TTL_MILLIS as u128
        {
            self.device_proximity_data_map.clear();
        }

        self.last_range_update_time = current_time_millis;
        if self
            .device_proximity_data_map
            .get(&proximity_estimate.device_id)
            == None
        {
            let new_device_proximity_data =
                DeviceProximityData::new(Some(self.proximity_state_options));
            let device_id = &proximity_estimate.device_id;
            self.device_proximity_data_map
                .insert(*device_id, new_device_proximity_data);
        }

        // Fix this code and remove hardcoded current proximity state. Error: Cannot borrow data in a & reference as mutable
        // let proximity_state_of_current_range = self.device_proximity_data_map.get(&proximity_estimate.device_id).unwrap().update_current_proximity_state(proximity_estimate);
        ProximityStateResult {
            device_id: proximity_estimate.device_id,
            current_proximity_state: ProximityState::Tap,
            current_proximity_estimate: proximity_estimate,
        }
    }
}
