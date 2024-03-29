// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// FFI interface of Presence Engine.
use client_provider;
use libc::size_t;

#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C)]
pub enum IdentityType {
    Private = 0,
    Trusted,
    Public,
}

#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C)]
pub enum MeasurementAccuracy {
    Unknown = 0,
    CoarseAccuracy,
    BestAvailable,
}
/// Struct to hold an action, identity type and their associated discovery condition.
#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct DiscoveryCondition {
    pub action: u32,
    pub identity_type: IdentityType,
    pub measurement_accuracy: MeasurementAccuracy,
}

/// Struct to hold a list of DiscoveryCondition.
///
/// The `count` is the numer of items in the list.
#[derive(Debug)]
#[repr(C)]
pub struct DiscoveryConditionList {
    pub items: *const DiscoveryCondition,
    pub count: size_t,
}

#[derive(Debug)]
#[repr(C)]
/// Struct to send a discovery request to the Engine.
pub struct DiscoveryEngineRequest {
    pub priority: i32,
    pub conditions: DiscoveryConditionList,
}

#[no_mangle]
/// Echoes back a [DiscoveryEngineRequest](struct.DiscoveryEngineRequest.html).
/// The `*request_ptr` is cloned to the returned result.
/// Caller owns both the input and output and is responsible to free the memory by calling
/// [free_engine_request()](fn.free_engine_request.html)
pub unsafe extern "C" fn echo_request(
    request_ptr: *const DiscoveryEngineRequest) -> *const DiscoveryEngineRequest {
    println!("Rust receives a DiscoveryEngineRequest.");
    client_provider::DiscoveryEngineRequest::from_raw(request_ptr).to_raw()
}

/// Free the memory associated with the [`DiscoveryEngineRequest`](struct.DiscoveryEngineRequest.html).
#[no_mangle]
pub unsafe extern "C" fn free_engine_request(request_ptr: *const DiscoveryEngineRequest) {
    if request_ptr.is_null() { return; }
    let ptr = request_ptr as *mut DiscoveryEngineRequest;
    let request: Box<DiscoveryEngineRequest> = Box::from_raw(ptr);
    let condition_list = request.conditions;
    let _conditions = Vec::from_raw_parts(
        condition_list.items as *mut DiscoveryCondition,
        condition_list.count as usize,
        condition_list.count as usize);
     // Resources will be freed here
}