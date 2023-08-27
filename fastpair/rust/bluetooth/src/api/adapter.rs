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

use crate::common::{BleAdvertisement, BleDataTypeId, BluetoothError};

/// Concrete types implementing this trait are Bluetooth Central devices.
/// They provide methods for retrieving nearby connections and device info.
#[async_trait]
pub trait BleAdapter: Sized {
    /// Retrieve the system-default Bluetooth adapter.
    async fn default() -> Result<Self, BluetoothError>;

    /// Begin scanning for nearby advertisements.
    fn start_scan(&mut self) -> Result<(), BluetoothError>;

    /// Stop scanning for nearby advertisements.
    fn stop_scan(&mut self) -> Result<(), BluetoothError>;

    /// Poll next discovered device.
    async fn next_advertisement(
        &mut self,
        data_selector: Option<&Vec<BleDataTypeId>>,
    ) -> Result<BleAdvertisement, BluetoothError>;
}
