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

const ADVERTISE_TX_POWER_HIGH_DB: i32 = 1;

const FSPL_AT_1_METER_DB: i32 = 40;

const MEASURED_POWER_AT_1_METER_DB_AT_HIGH_TX_POWER: i32 = -60;

pub fn compute_distance_meters_at_high_tx_power(rssi: i32) -> f64 {
    let nominal_tx_power = ADVERTISE_TX_POWER_HIGH_DB;
    let antenna_gain =
        (nominal_tx_power - FSPL_AT_1_METER_DB) - MEASURED_POWER_AT_1_METER_DB_AT_HIGH_TX_POWER;
    let tx_power_at_0_meters = nominal_tx_power - antenna_gain;
    compute_distance_meters(tx_power_at_0_meters, rssi)
}

pub fn compute_distance_meters(tx_power_at_0_meters: i32, rssi: i32) -> f64 {
    let fspl = tx_power_at_0_meters - rssi;
    ble_fspl_to_meters(fspl)
}

fn ble_fspl_to_meters(fspl: i32) -> f64 {
    let base: f64 = 10.0;
    base.powi((fspl - FSPL_AT_1_METER_DB) / 20)
}
