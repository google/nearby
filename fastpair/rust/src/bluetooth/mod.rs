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

// Split into separate crate once demo is finished, providing custom error types
// instead of using anyhow.
// b/290070686

pub mod common;

pub use common::{Adapter, Device};

cfg_if::cfg_if! {
    if #[cfg(windows)] {
        mod windows_ble;
        use windows_ble::BleAdapter;
    } else {
        mod unsupported;
        use unsupported::BleAdapter;
    }
}

pub async fn default_adapter() -> Result<impl Adapter, anyhow::Error> {
    BleAdapter::default().await
}
