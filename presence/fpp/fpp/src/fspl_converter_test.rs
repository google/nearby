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

use crate::fspl_converter::compute_distance_meters_at_high_tx_power;

#[test]
fn test_short_distance() {
    assert_eq!(compute_distance_meters_at_high_tx_power(-40), 0.1);
}

#[test]
fn test_medium_distance() {
    assert_eq!(compute_distance_meters_at_high_tx_power(-60), 1.0);
}

#[test]
fn test_large_distance() {
    assert_eq!(compute_distance_meters_at_high_tx_power(-80), 10.0);
}
