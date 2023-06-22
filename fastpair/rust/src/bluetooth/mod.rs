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

cfg_if::cfg_if! {
    if #[cfg(windows)] {
        mod windows_ble;
        use windows_ble as imp;
    } else {
        compile_error!("Unsupported target platform. Currently supported: Windows.");
    }
}

pub struct BleAdapter {
    inner: imp::BleAdapter,
}

impl BleAdapter {
    pub async fn default() -> Result<Self, anyhow::Error> {
        let inner = imp::BleAdapter::default().await?;
        Ok(BleAdapter { inner })
    }
}

pub struct BleDevice {
    inner: imp::BleDevice,
}

impl BleDevice {
    pub fn name(&self) -> Result<String, anyhow::Error> {
        self.inner.name()
    }
}
