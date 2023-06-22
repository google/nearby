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

use windows::Devices::Bluetooth::BluetoothAdapter;

pub struct BleAdapter {
    inner: BluetoothAdapter,
}

impl BleAdapter {
    pub async fn default() -> Result<Self, anyhow::Error> {
        let inner = BluetoothAdapter::GetDefaultAsync()?.await?;

        if !inner.IsLowEnergySupported()? {
            return Err(anyhow::anyhow!(
                "This device's Bluetooth Adapter doesn't support Bluetooth LE Transport type."
            ));
        }
        if !inner.IsCentralRoleSupported()? {
            return Err(anyhow::anyhow!(
                "This device's Bluetooth Adapter doesn't support Bluetooth LE central role."
            ));
        }

        Ok(BleAdapter { inner })
    }
}

mod tests {
    use super::*;

    // TODO b/288592509 unit tests
}
