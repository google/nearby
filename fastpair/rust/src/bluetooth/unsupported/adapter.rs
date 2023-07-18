// Copyright 2023 Google LLC
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

use async_trait::async_trait;

use super::BleDevice;
use crate::bluetooth::common::{Adapter, BluetoothError};

/// Concrete type implementing `Adapter`, used for unsupported devices.
/// Every method should panic.
pub struct BleAdapter;

#[async_trait]
impl Adapter for BleAdapter {
    type Device = BleDevice;

    async fn default() -> Result<Self, BluetoothError> {
        panic!("Unsupported target platform.");
    }

    fn start_scan_devices(&mut self) -> Result<(), BluetoothError> {
        panic!("Unsupported target platform.");
    }

    fn stop_scan_devices(&mut self) -> Result<(), BluetoothError> {
        panic!("Unsupported target platform.");
    }

    async fn next_device(&mut self) -> Result<Self::Device, BluetoothError> {
        panic!("Unsupported target platform.");
    }
}

mod tests {
    use super::*;

    // TODO b/288592509 unit tests
}
