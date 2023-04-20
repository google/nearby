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

use fpp::fused_presence_utils::*;
use fpp::presence_detector::*;

// C-compatible API
#[no_mangle]
pub unsafe extern "C" fn presence_detector_create() -> *mut PresenceDetector {
    Box::into_raw(Box::new(PresenceDetector::new()))
}

#[no_mangle]
pub unsafe extern "C" fn update_ble_scan_result(
    presence_detector: *mut PresenceDetector,
    ble_scan_result: BleScanResult,
    proximity_state_result: *mut ProximityStateResult,
) {
    if let Some(presence_detector) = presence_detector.as_mut() {
        if let Some(proximity_state_result) = proximity_state_result.as_mut() {
            let result = presence_detector.on_ble_scan_result(ble_scan_result);
            if let Some(result) = result {
                *proximity_state_result = result;
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn fpp_configure_options(
    presence_detector: *mut PresenceDetector,
    proximity_state_options: ProximityStateOptions,
) {
    if let Some(presence_detector) = presence_detector.as_mut() {
        presence_detector.configure_proximity_state_options(proximity_state_options);
    }
}

#[no_mangle]
pub unsafe extern "C" fn fpp_get_proximity_estimate(
    presence_detector: *mut PresenceDetector,
    device_id: u64,
    proximity_estimate: *mut ProximityEstimate,
) {
    if let Some(presence_detector) = presence_detector.as_mut() {
        if presence_detector.get_proximity_estimate(device_id) != None {
            if let Some(proximity_estimate) = proximity_estimate.as_mut() {
                *proximity_estimate = presence_detector.get_proximity_estimate(device_id).unwrap();
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn presence_detector_free(
    presence_detector: *mut PresenceDetector,
) -> std::os::raw::c_int {
    if !presence_detector.is_null() {
        let _ = Box::from_raw(presence_detector);
        return 0;
    }
    return -1;
}
