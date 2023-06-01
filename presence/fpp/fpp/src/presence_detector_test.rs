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

use crate::fused_presence_utils::*;
use crate::presence_detector::*;

const BLE_SCAN_RESULT_REACH_ZONE: BleScanResult = BleScanResult {
    device_id: 1234,
    tx_power: { MaybeTxPower::Invalid },
    rssi: -40,
    elapsed_real_time_millis: 123456,
};

const BLE_SCAN_RESULT_BAD_RSSI: BleScanResult = BleScanResult {
    rssi: 127,
    ..BLE_SCAN_RESULT_REACH_ZONE
};

const BLE_SCAN_RESULT_SHORT_RANGE_ZONE: BleScanResult = BleScanResult {
    rssi: -60,
    ..BLE_SCAN_RESULT_REACH_ZONE
};

const REACH_PROXIMITY_ESTIMATE: ProximityEstimate = ProximityEstimate {
    device_id: 1234,
    distance_meters: 0.1,
    distance_confidence: MeasurementConfidence::Low,
    elapsed_real_time_millis: 0,
    proximity_state: ProximityState::Reach,
    source: PresenceDataSource::Ble,
};

const SHORT_RANGE_PROXIMITY_ESTIMATE: ProximityEstimate = ProximityEstimate {
    distance_meters: 1.0,
    proximity_state: ProximityState::ShortRange,
    ..REACH_PROXIMITY_ESTIMATE
};

#[test]
fn test_on_ble_scan_result_success() {
    // Tests that the proximity state stored for each device is the accurate one after two
    // consecutive scan results
    let mut presence_detector = PresenceDetector::new();
    assert_eq!(
        presence_detector.on_ble_scan_result(BLE_SCAN_RESULT_REACH_ZONE),
        None
    );
    assert_eq!(
        presence_detector.on_ble_scan_result(BLE_SCAN_RESULT_REACH_ZONE),
        Some(ProximityEstimate {
            device_id: 1234,
            distance_meters: 0.1,
            distance_confidence: MeasurementConfidence::Low,
            elapsed_real_time_millis: 0,
            proximity_state: ProximityState::Reach,
            source: PresenceDataSource::Ble
        })
    );
}

#[test]
fn test_on_ble_scan_result_bad_rssi() {
    // Tests that scan results with bad RSSIs are ignored
    let mut presence_detector = PresenceDetector::new();
    assert_eq!(
        presence_detector.on_ble_scan_result(BLE_SCAN_RESULT_REACH_ZONE),
        None
    );

    assert_eq!(
        presence_detector.on_ble_scan_result(BLE_SCAN_RESULT_BAD_RSSI),
        None
    );
}
#[test]
fn test_on_ble_scan_result_transition_to_new_zone() {
    let mut presence_detector = PresenceDetector::new();
    assert_eq!(
        presence_detector.on_ble_scan_result(BLE_SCAN_RESULT_REACH_ZONE),
        None
    );
    assert_eq!(
        presence_detector.on_ble_scan_result(BLE_SCAN_RESULT_REACH_ZONE),
        Some(REACH_PROXIMITY_ESTIMATE)
    );
    assert_eq!(
        presence_detector.on_ble_scan_result(BLE_SCAN_RESULT_SHORT_RANGE_ZONE),
        Some(REACH_PROXIMITY_ESTIMATE)
    );
    assert_eq!(
        presence_detector.on_ble_scan_result(BLE_SCAN_RESULT_SHORT_RANGE_ZONE),
        Some(SHORT_RANGE_PROXIMITY_ESTIMATE)
    );
}
