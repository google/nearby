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

use crate::common::{
    BleAddress, BluetoothError, ClassicAddress, PairingResult,
};

/// Concrete types implementing this trait represent BLE Peripheral devices.
/// They provide methods for retrieving device info and running device actions,
/// such as pairing.
#[async_trait]
pub trait BleDevice: Sized {
    /// Create a new `BleDevice` instance from a `BleAddress`, typically
    /// enabled through locally cached data retrieved from a Bluetooth adapter's
    /// scanning functionality.
    async fn new(addr: BleAddress) -> Result<Self, BluetoothError>;

    /// Retrieve the name advertised by this device.
    fn name(&self) -> Result<String, BluetoothError>;

    /// Retrieve this device's Bluetooth address information.
    fn address(&self) -> BleAddress;
}

/// Concrete types implementing this trait represent BT Classic Peripheral
/// devices. They provide methods for retrieving device info and running device
/// actions, such as pairing.
#[async_trait]
pub trait ClassicDevice: Sized {
    /// Create a new `ClassicDevice` instance from a `ClassicAddress`, typically
    /// enabled through locally cached data retrieved from a Bluetooth adapter's
    /// scanning functionality.
    async fn new(addr: ClassicAddress) -> Result<Self, BluetoothError>;

    /// Retrieve the name advertised by this device.
    fn name(&self) -> Result<String, BluetoothError>;

    /// Retrieve this device's Bluetooth address information.
    fn address(&self) -> ClassicAddress;

    /// Attempt pairing with the peripheral device.
    async fn pair(&self) -> Result<PairingResult, BluetoothError>;
}
