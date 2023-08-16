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

use crate::{
    advertisement::ModelId,
    fetcher::{DeviceInfo, FpFetcher, JsonData},
};

/// A struct for retrieving Fast Pair information from the local filesystem.
pub(crate) struct FpFetcherFs {
    path: String,
}

impl FpFetcherFs {
    pub(crate) fn new(path: String) -> Self {
        FpFetcherFs { path }
    }
}

impl FpFetcher for FpFetcherFs {
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

        Ok(model_info.device())
    }
}
