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

use crate::{
    api,
    common::{BleAddress, BluetoothError, ClassicAddress, PairingResult},
};

/// Concrete type implementing `api::BleDevice` for unsupported platforms.
/// Every method should panic.
pub struct BleDevice;

#[async_trait]
impl api::BleDevice for BleDevice {
    async fn new(addr: BleAddress) -> Result<Self, BluetoothError> {
        panic!("Unsupported target platform.");
    }

    fn name(&self) -> Result<String, BluetoothError> {
        panic!("Unsupported target platform.");
    }

    fn address(&self) -> BleAddress {
        panic!("Unsupported target platform.");
    }
}

/// Concrete type implementing `api::ClassicDevice` for unsupported platforms.
/// Every method should panic.
pub struct ClassicDevice;

#[async_trait]
impl api::ClassicDevice for ClassicDevice {
    async fn new(addr: ClassicAddress) -> Result<Self, BluetoothError> {
        panic!("Unsupported target platform.");
    }

    fn name(&self) -> Result<String, BluetoothError> {
        panic!("Unsupported target platform.");
    }

    fn address(&self) -> ClassicAddress {
        panic!("Unsupported target platform.");
    }

    async fn pair(&self) -> Result<PairingResult, BluetoothError> {
        panic!("Unsupported target platform.");
    }
}

mod tests {
    use super::*;

    // TODO b/288592509 unit tests
}
