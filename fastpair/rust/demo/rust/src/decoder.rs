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

use bluetooth::ServiceData;

/// Unit struct providing parsing operations for Fast Pair advertisements.
pub(crate) struct FpDecoder;

impl FpDecoder {
    /// Retrieve the Fast Pair device model ID from a service data payload.
    /// https://developers.google.com/nearby/fast-pair/specifications/service/provider.
    /// * Length < 3: invalid payload
    /// * Length == 3: entire payload is the model ID
    /// * Length > 3: first byte specifies the length of the model ID, in bytes.
    /// Currently unavailable in Fast Pair devices and not supported.
    pub(crate) fn get_model_id_from_service_data<U: Copy>(
        service_data: &ServiceData<U>,
    ) -> Result<Vec<u8>, anyhow::Error> {
        static MIN_MODEL_ID_LENGTH: usize = 3;
        let data = service_data.data();

        if data.len() < MIN_MODEL_ID_LENGTH {
            // If service data too small, invalid payload.
            Err(anyhow::anyhow!(format!(
                "Invalid model ID for Fast Pair advertisement of length {}.",
                data.len()
            )))
        } else if data.len() == MIN_MODEL_ID_LENGTH {
            // Else if service data length is exactly 3, all bytes are the ID.
            Ok(data.clone())
        } else {
            // Else, this Fast Pair advertisement is currently unsupported.
            // b/294453912
            Err(anyhow::anyhow!(
                "This Fast Pair device is currently unsupported."
            ))
        }
    }
}
