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

use super::{BleAddress, BluetoothError};

/// Holds data related to an incoming BLE Advertisement. This includes
/// information about the advertisement (e.g. address of sender) as well as
/// data sections extracted from the advertisement. Platform-specific methods
/// should be written to load in data sections from incoming advertisements.
pub struct BleAdvertisement {
    address: BleAddress,
    service_data_16bit_uuid: Option<Vec<ServiceData<u16>>>,
}

impl BleAdvertisement {
    /// Construct a new `BleAdvertisement` instance.
    pub(crate) fn new(address: BleAddress) -> Self {
        BleAdvertisement {
            address,
            service_data_16bit_uuid: None,
        }
    }

    /// Retrieve the `BleAddress` that emitted this advertisement.
    pub fn address(&self) -> BleAddress {
        self.address
    }

    /// Setter for `ServiceData` field with 16bit UUID.
    pub(crate) fn set_service_data_16bit_uuid(
        &mut self,
        data_sections: Vec<ServiceData<u16>>,
    ) {
        self.service_data_16bit_uuid = Some(data_sections);
    }

    /// Getter for `ServiceData` field with 16bit UUID.
    pub fn service_data_16bit_uuid(
        &self,
    ) -> Result<&Vec<ServiceData<u16>>, BluetoothError> {
        match &self.service_data_16bit_uuid {
            Some(service_data) => Ok(&service_data),
            None => Err(BluetoothError::FailedPrecondition(String::from(
                "No service data has been loaded into this advertisement.",
            ))),
        }
    }
}

/// Enum denoting the assigned number of Bluetooth common data types. Used for
/// fetching specific data sections from a Bluetooth advertisement.
/// Bluetooth Assigned Numbers, Section 2.3
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
pub enum BleDataTypeId {
    ServiceData16BitUuid = 0x16,
}

/// Struct representing the Bluetooth Service Data common data type. `U` should
/// be one of the valid uuid sizes, specified in:
/// Bluetooth Supplement to the Core Specification, Part A, Section 1.11.
pub struct ServiceData<U: Copy> {
    uuid: U,
    data: Vec<u8>,
}

impl<U: Copy> ServiceData<U> {
    pub fn new(uuid: U, data: Vec<u8>) -> Self {
        ServiceData { uuid, data }
    }

    pub fn uuid(&self) -> U {
        self.uuid
    }

    pub fn data(&self) -> &Vec<u8> {
        &self.data
    }
}
