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

#![deny(
    missing_docs,
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::panic,
    clippy::expect_used
)]

//! Rust FFI wrapper for PresenceDetector. Can be called from C/C++ clients

use fpp::fused_presence_utils::*;
use fpp::presence_detector::*;

use crate::handle_map::get_presence_detector_handle_map;

mod handle_map;

/// Wraps the handle ID to an underlying PresenceDetector object
#[repr(C)]
pub struct PresenceDetectorHandle {
    handle: u64,
}

/// Error enum class representing possible errors
#[repr(C)]
pub enum ProximityEstimateError {
    /// Returned if the handle is invalid
    InvalidPresenceDetectorHandle,
    /// Returned if the output parameter is null
    NullOutputParameter,
}

impl ProximityEstimateError {
    fn to_error_code(&self) -> i32 {
        match self {
            Self::InvalidPresenceDetectorHandle => -1,
            Self::NullOutputParameter => -2,
        }
    }
}

const SUCCESS: i32 = 0;

/// Creates a new presence detector object and returns the handle for the new object
#[no_mangle]
pub extern "C" fn presence_detector_create() -> PresenceDetectorHandle {
    let handle = get_presence_detector_handle_map().insert(Box::new(PresenceDetector::new()));
    PresenceDetectorHandle { handle }
}

/// Updates PresenceDetector with a new scan result and returns an error code if unsuccessful
///
/// # Safety
///
/// Ensure that the output parameter refers to an initialized instance
#[no_mangle]
pub unsafe extern "C" fn update_ble_scan_result(
    presence_detector_handle: PresenceDetectorHandle,
    ble_scan_result: BleScanResult,
    proximity_estimate: *mut ProximityEstimate,
) -> i32 {
    if let Some(presence_detector) =
        get_presence_detector_handle_map().get(&presence_detector_handle.handle)
    {
        presence_detector
            .on_ble_scan_result(ble_scan_result)
            .map(|current_proximity_estimate| {
                proximity_estimate.as_mut().map(|proximity_estimate| {
                    *proximity_estimate = current_proximity_estimate;
                    Some(SUCCESS)
                });
                ProximityEstimateError::NullOutputParameter.to_error_code()
            });
    }
    ProximityEstimateError::InvalidPresenceDetectorHandle.to_error_code()
}

/// Gets the current proximity estimate for a given device ID
///
/// # Safety
///
/// Ensure that the output parameter refers to an initialized instance
#[no_mangle]
pub unsafe extern "C" fn get_proximity_estimate(
    presence_detector_handle: PresenceDetectorHandle,
    device_id: u64,
    proximity_estimate: *mut ProximityEstimate,
) -> i32 {
    if let Some(presence_detector) =
        get_presence_detector_handle_map().get(&presence_detector_handle.handle)
    {
        presence_detector
            .get_proximity_estimate(device_id)
            .map(|current_proximity_estimate| {
                if let Some(proximity_estimate) = proximity_estimate.as_mut() {
                    *proximity_estimate = current_proximity_estimate;
                    return SUCCESS;
                }
                ProximityEstimateError::NullOutputParameter.to_error_code()
            });
    }

    ProximityEstimateError::InvalidPresenceDetectorHandle.to_error_code()
}

/// De-allocates memory for a presence detector object
#[no_mangle]
pub extern "C" fn presence_detector_free(
    presence_detector_handle: PresenceDetectorHandle,
) -> std::os::raw::c_int {
    if let Some(presence_detector) =
        get_presence_detector_handle_map().remove(&presence_detector_handle.handle)
    {
        let _ = *presence_detector;
        return SUCCESS;
    }
    ProximityEstimateError::InvalidPresenceDetectorHandle.to_error_code()
}
