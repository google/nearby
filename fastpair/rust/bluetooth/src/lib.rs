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

pub mod api;
mod common;

use api::{BleAdapter, BleDevice, ClassicDevice};
pub use common::{
    BleAddress, BleAddressKind, BleAdvertisement, BleDataTypeId,
    BluetoothError, ClassicAddress, PairingResult, ServiceData,
};

cfg_if::cfg_if! {
    if #[cfg(windows)] {
        mod windows;
        use self::windows as platform;
    } else {
        mod unsupported;
        use unsupported as platform;
    }
}

pub struct Platform;

impl Platform {
    pub async fn default_adapter(
    ) -> Result<impl api::BleAdapter, BluetoothError> {
        platform::BleAdapter::default().await
    }

    pub async fn new_ble_device(
        addr: BleAddress,
    ) -> Result<impl api::BleDevice, BluetoothError> {
        platform::BleDevice::new(addr).await
    }

    pub async fn new_classic_device(
        addr: ClassicAddress,
    ) -> Result<impl api::ClassicDevice, BluetoothError> {
        platform::ClassicDevice::new(addr).await
    }
}
