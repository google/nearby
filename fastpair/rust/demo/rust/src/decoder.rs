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

use crate::error::FpError;

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
    ) -> Result<Vec<u8>, FpError> {
        const MIN_MODEL_ID_LENGTH: usize = 3;
        let data = service_data.data();

        if data.len() < MIN_MODEL_ID_LENGTH {
            // If service data too small, invalid payload.
            Err(FpError::ContractViolation(format!(
                "Invalid model ID for Fast Pair advertisement of length {}.",
                data.len()
            )))
        } else if data.len() == MIN_MODEL_ID_LENGTH {
            // Else if service data length is exactly 3, all bytes are the ID.
            Ok(data.clone())
        } else {
            // Else, this Fast Pair advertisement is currently unsupported.
            // b/294453912
            Err(FpError::NotImplemented(String::from(
                "This Fast Pair device is currently unsupported.",
            )))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_get_model_id_valid() {
        // Valid scenario: Length == 3
        let uuid: u16 = 0x1234;
        let data = vec![0xAA, 0xBB, 0xCC];
        let service_data = ServiceData::new(uuid, data.clone());
        let result = FpDecoder::get_model_id_from_service_data(&service_data);
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), data);
    }

    #[test]
    fn test_get_model_id_invalid() {
        // Invalid scenario: Length < 3
        let uuid: u16 = 0x1234;
        let data = vec![0xAA, 0xBB];
        let service_data = ServiceData::new(uuid, data);
        let result = FpDecoder::get_model_id_from_service_data(&service_data);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), FpError::ContractViolation(_)));
    }

    #[test]
    fn test_get_model_id_unsupported() {
        // Unsupported scenario: Length > 3
        let uuid: u16 = 0x1234;
        let data = vec![0xAA, 0xBB, 0xCC, 0xDD];
        let service_data = ServiceData::new(uuid, data);
        let result = FpDecoder::get_model_id_from_service_data(&service_data);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), FpError::NotImplemented(_)));
    }
}
