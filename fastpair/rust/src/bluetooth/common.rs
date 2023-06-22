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

use std::pin::Pin;

use async_trait::async_trait;
use futures::stream::Stream;

/// Concrete types implementing this trait are Bluetooth Central devices.
/// They provide methods for retrieving nearby connections and device info.
#[async_trait]
pub trait Adapter: Sized {
    type Device: Device;

    /// Retrieve the system-default Bluetooth adapter.
    async fn default() -> Result<Self, anyhow::Error>;

    /// Scan for nearby devices, returning a `Stream` of futures that can be
    /// iterated over and polled to retrieve `BleDevice`.
    //
    // NOTE: Using Boxed dyn here is silly because in cross-platform code there
    // should only ever be one concrete type implementing Adapter. Change this
    // to `impl Stream` once impl trait return types are stabilized in traits.
    // b/289224233.
    fn scan_devices(&self) -> Result<Pin<Box<dyn Stream<Item = Self::Device>>>, anyhow::Error>;
}

/// Concrete types implementing this trait represent Bluetooth Peripheral devices.
/// They provide methods for retrieving device info and running device actions,
/// such as pairing.
pub trait Device {
    /// Retrieve the name advertised by this device.
    fn name(&self) -> Result<String, anyhow::Error>;
}
