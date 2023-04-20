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

use lazy_static::lazy_static;

const ADVERTISE_TX_POWER_ULTRA_LOW: u8 = 0;
const ADVERTISE_TX_POWER_LOW: u8 = 1;
const ADVERTISE_TX_POWER_MEDIUM: u8 = 2;
const ADVERTISE_TX_POWER_HIGH: u8 = 3;

const FSPL_AT_1_METER_DB: i16 = 40;

const MEASURED_POWER_AT_1_METER_DB_AT_HIGH_TX_POWER: i8 = -60;

lazy_static! {
  static ref TX_POWER_SETTING_TO_DB: HashMap<u8, i8> = {
    let mut m = HashMap::new();
    // Nominal power requested to antenna when using different Advertising TX Power settings
    m.insert(ADVERTISE_TX_POWER_HIGH, 1);
    m.insert(ADVERTISE_TX_POWER_MEDIUM, -7);
    m.insert(ADVERTISE_TX_POWER_LOW, -15);
    m.insert(ADVERTISE_TX_POWER_ULTRA_LOW, -21);
    m
  };
}

pub fn compute_distance_meters_at_high_tx_power(rssi: i16) -> f64 {
    let nominal_tx_power = TX_POWER_SETTING_TO_DB.get(&ADVERTISE_TX_POWER_HIGH);
    let antenna_gain = (nominal_tx_power.unwrap() - FSPL_AT_1_METER_DB as i8)
        - MEASURED_POWER_AT_1_METER_DB_AT_HIGH_TX_POWER;
    let tx_power_at_0_meters = nominal_tx_power.unwrap() - antenna_gain;
    compute_distance_meters(tx_power_at_0_meters as i16, rssi)
}

pub fn compute_distance_meters(tx_power_at_0_meters: i16, rssi: i16) -> f64 {
    let fspl: i16 = tx_power_at_0_meters - rssi;
    ble_fspl_to_meters(fspl)
}

fn ble_fspl_to_meters(fspl: i16) -> f64 {
    let base: f64 = 10.0;
    base.powi((fspl - FSPL_AT_1_METER_DB) as i32 / 20)
}
