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
use std::fs;

use serde::Deserialize;

use crate::advertisement::ModelId;

/// Holds Fast Pair device information parsed from JSON.
#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
pub(crate) struct DeviceInfo {
    image_url: String,
    name: String,
}

/// Holds top-level Fast Pair information parsed from JSON. See `local`
/// directory for format.
#[derive(Deserialize)]
struct JsonData {
    device: DeviceInfo,
}

/// Types that can fetch Fast Pair data from external storage (e.g. filesystem,
/// remote server).
pub(crate) trait FpFetcher {
    fn get_device_info_from_model_id(
        &self,
        model_id: &ModelId,
    ) -> Result<DeviceInfo, anyhow::Error>;
}

/// A unit struct for retrieving Fast Pair information from the local filesystem.
pub(crate) struct FpFetcherLocal {
    path: String,
}

impl FpFetcherLocal {
    pub(crate) fn new(path: String) -> Self {
        FpFetcherLocal { path }
    }
}

impl FpFetcher for FpFetcherLocal {
    /// Retrieve device information for the provided Model ID. Currently,
    /// this information is saved locally. In the future, this should instead
    /// be retrieved from a remote server and cached.
    /// b/294456411
    fn get_device_info_from_model_id(
        &self,
        model_id: &ModelId,
    ) -> Result<DeviceInfo, anyhow::Error> {
        let file_path = format!("{}/{}.json", self.path, model_id);
        let contents = fs::read_to_string(file_path).expect("Couldn't find or open file.");

        let model_info: JsonData = serde_json::from_str(&contents)?;

        Ok(model_info.device)
    }
}

impl DeviceInfo {
    pub(crate) fn name(&self) -> &String {
        &self.name
    }

    pub(crate) fn image_url(&self) -> &String {
        &self.image_url
    }
}
