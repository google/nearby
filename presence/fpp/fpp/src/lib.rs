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

//! Processes raw scan results from BLE, UWB and NAN and outputs proximity estimates/zones

mod fspl_converter;

/// Fused presence Utils
pub mod fused_presence_utils;

/// Presence detector module
pub mod presence_detector;

#[cfg(test)]
mod fspl_converter_test;

#[cfg(test)]
mod presence_detector_test;
