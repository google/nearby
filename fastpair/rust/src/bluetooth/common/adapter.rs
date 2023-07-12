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

use super::Device;

/// Concrete types implementing this trait are Bluetooth Central devices.
/// They provide methods for retrieving nearby connections and device info.
#[async_trait]
pub trait Adapter: Sized {
    type Device: Device;

    /// Retrieve the system-default Bluetooth adapter.
    async fn default() -> Result<Self, anyhow::Error>;

    /// Begin scanning for nearby devices.
    fn start_scan_devices(&mut self) -> Result<(), anyhow::Error>;

    /// Stop scanning for nearby devices.
    fn stop_scan_devices(&mut self) -> Result<(), anyhow::Error>;

    /// Poll next discovered device.
    async fn next_device(&mut self) -> Result<Self::Device, anyhow::Error>;
}
