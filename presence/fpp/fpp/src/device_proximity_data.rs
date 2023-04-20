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

use std::collections::vec_deque::VecDeque;

use crate::fused_presence_utils::*;

// Static function for biasing the prediction to the current state to reduce
// state changes due to noisy signal
fn calculate_proximity_state_hysteresis(
    distance_meters: f64,
    last_reported_proximity_state: Option<ProximityState>,
    proximity_state_options: ProximityStateOptions,
) -> ProximityState {
    let enlarge_multiplier = 1.0 + proximity_state_options.hysteresis_percentage;
    let shorten_multiplier = 1.0 - proximity_state_options.hysteresis_percentage;
    let mut tap_threshold = proximity_state_options.tap_threshold_meters;
    let mut reach_threshold = proximity_state_options.reach_threshold_meters;
    let mut short_range_threshold = proximity_state_options.short_range_threshold_meters;
    let mut long_range_threshold = proximity_state_options.long_range_threshold_meters;

    match last_reported_proximity_state {
        Some(ref _proximity_state) => match last_reported_proximity_state.unwrap() {
            ProximityState::Tap => {
                tap_threshold = tap_threshold * enlarge_multiplier;
                tap_threshold =
                    tap_threshold + proximity_state_options.hysteresis_tap_extra_debounce_meters;
            }
            ProximityState::Reach => {
                tap_threshold = tap_threshold * shorten_multiplier;
                reach_threshold = reach_threshold * enlarge_multiplier;
            }
            ProximityState::ShortRange => {
                reach_threshold = reach_threshold * shorten_multiplier;
                short_range_threshold = short_range_threshold * enlarge_multiplier;
            }
            ProximityState::LongRange => {
                short_range_threshold = short_range_threshold * shorten_multiplier;
                long_range_threshold = long_range_threshold * enlarge_multiplier;
            }
            ProximityState::Far => {}
            ProximityState::Unknown => {}
        },
        None => {}
    }
    get_proximity_state_from_threshold(
        distance_meters,
        tap_threshold,
        reach_threshold,
        short_range_threshold,
        long_range_threshold,
    )
}

// Static function for getting proximity state from threshold
fn get_proximity_state_from_threshold(
    distance_meters: f64,
    tap_threshold: f64,
    reach_threshold: f64,
    short_range_threshold: f64,
    long_range_threshold: f64,
) -> ProximityState {
    if distance_meters <= tap_threshold {
        return ProximityState::Tap;
    }
    if distance_meters <= reach_threshold {
        return ProximityState::Reach;
    }
    if distance_meters <= short_range_threshold {
        return ProximityState::ShortRange;
    }
    if distance_meters <= long_range_threshold {
        return ProximityState::LongRange;
    }
    ProximityState::Far
}

#[derive(PartialEq)]
pub struct DeviceProximityData {
    device_distance_meters_history: VecDeque<f64>,
    proximity_state_options: ProximityStateOptions,
    transition_history: VecDeque<ProximityState>,
    current_proximity_state: ProximityState,
    source: PresenceDataSource,
}

impl DeviceProximityData {
    pub fn new(proximity_state_options: Option<ProximityStateOptions>) -> Self {
        let mut options = ProximityStateOptions {
            tap_threshold_meters: DEFAULT_TAP_DISTANCE_THRESHOLD_METERS,
            reach_threshold_meters: DEFAULT_REACH_DISTANCE_THRESHOLD_METERS,
            short_range_threshold_meters: DEFAULT_SHORT_RANGE_DISTANCE_THRESHOLD_METERS,
            long_range_threshold_meters: DEFAULT_LONG_RANGE_DISTANCE_THRESHOLD_METERS,
            hysteresis_percentage: DEFAULT_HYSTERESIS_PERCENTAGE,
            hysteresis_tap_extra_debounce_meters: DEFAULT_HYSTERESIS_TAP_EXTRA_DEBOUNCE_METERS,
            consecutive_scans_required: DEFAULT_CONSECUTIVE_SCANS_REQUIRED,
        };
        if proximity_state_options != None {
            options = proximity_state_options.unwrap();
        }
        let storage_length = options.consecutive_scans_required + 1;
        DeviceProximityData {
            device_distance_meters_history: VecDeque::with_capacity(storage_length.into()),
            proximity_state_options: options,
            transition_history: VecDeque::with_capacity(storage_length.into()),
            current_proximity_state: ProximityState::Unknown,
            source: PresenceDataSource::Unknown,
        }
    }

    pub fn get_source(&self) -> &PresenceDataSource {
        &self.source
    }

    pub fn get_current_proximity_state(&self) -> &ProximityState {
        &self.current_proximity_state
    }

    pub fn configure_options(&mut self, proximity_state_options: ProximityStateOptions) {
        self.proximity_state_options = proximity_state_options;
        let old_transition_history = &self.transition_history;
        let old_device_distance_meters_history = &self.device_distance_meters_history;
        let storage_length = proximity_state_options.consecutive_scans_required + &1;
        let mut new_transition_history: VecDeque<ProximityState> =
            VecDeque::with_capacity(storage_length.into());
        let mut new_device_distance_meters_history: VecDeque<f64> =
            VecDeque::with_capacity(storage_length.into());
        for history in old_transition_history {
            new_transition_history.push_front(*history);
            new_transition_history.truncate(
                self.proximity_state_options
                    .consecutive_scans_required
                    .into(),
            );
        }
        for distance_meters_history in old_device_distance_meters_history {
            new_device_distance_meters_history.push_front(*distance_meters_history);
            new_device_distance_meters_history.truncate(
                self.proximity_state_options
                    .consecutive_scans_required
                    .into(),
            );
        }
    }

    pub fn update_current_proximity_state(
        &mut self,
        proximity_estimate: ProximityEstimate,
    ) -> &ProximityState {
        let last_reported_proximity_state = self.current_proximity_state;
        let new_proximity_state = calculate_proximity_state_hysteresis(
            proximity_estimate.distance,
            Some(last_reported_proximity_state),
            self.proximity_state_options,
        );
        self.transition_history.push_front(new_proximity_state);
        self.transition_history.truncate(
            self.proximity_state_options
                .consecutive_scans_required
                .into(),
        );
        if self.transition_history.len()
            == self
                .proximity_state_options
                .consecutive_scans_required
                .into()
        {
            self.current_proximity_state = new_proximity_state;
            self.source = proximity_estimate.source;
        }
        &self.current_proximity_state
    }
}
