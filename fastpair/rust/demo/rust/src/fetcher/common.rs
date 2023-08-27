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

use serde::Deserialize;

use crate::{advertisement::ModelId, error::FpError};

/// Types that can fetch Fast Pair data from external storage (e.g. filesystem,
/// remote server).
pub(crate) trait FpFetcher {
    fn get_device_info_from_model_id(&self, model_id: &ModelId) -> Result<DeviceInfo, FpError>;
}

/// Holds Fast Pair device information parsed from JSON.
#[derive(Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct DeviceInfo {
    image_url: String,
    name: String,
}

/// Holds top-level Fast Pair information parsed from JSON. See `local`
/// directory for format.
#[derive(Deserialize)]
pub(super) struct JsonData {
    device: DeviceInfo,
}

impl DeviceInfo {
    // `new()` is conceivably only used in tests, since this struct should be
    // constructed by serde_json. Therefore, adding cfg to disable dead code
    // warnings. Can be removed in the future.
    #[cfg(test)]
    pub(crate) fn new(image_url: String, name: String) -> Self {
        DeviceInfo { image_url, name }
    }

    pub(crate) fn name(&self) -> &String {
        &self.name
    }

    pub(crate) fn image_url(&self) -> &String {
        &self.image_url
    }
}

impl JsonData {
    // Returns the `DeviceInfo` associated with parsed self, consuming self.
    pub(super) fn device(self) -> DeviceInfo {
        self.device
    }
}
