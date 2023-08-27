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
#[derive(Clone, Debug)]
pub struct BleAdvertisement {
    address: BleAddress,
    rssi: Option<DecibelMilliwatts>,
    tx_power: Option<DecibelMilliwatts>,
    service_data_16bit_uuid: Option<Vec<ServiceData<u16>>>,
}

/// Decibel-milliwatt or dBm is a dimensionless absolute unit expressing the
/// power of a signal relative to one milliwatt (mW). The unit is in log10, i.e.
/// 1 mW is 0 dBm and a 10 dBm increase represents a ten-fold increase in power.
type DecibelMilliwatts = i16;

impl BleAdvertisement {
    /// Construct a new `BleAdvertisement` instance.
    pub fn new(
        address: BleAddress,
        rssi: Option<DecibelMilliwatts>,
        tx_power: Option<DecibelMilliwatts>,
    ) -> Self {
        BleAdvertisement {
            address,
            rssi,
            tx_power,
            service_data_16bit_uuid: None,
        }
    }

    /// Retrieve the `BleAddress` that emitted this advertisement.
    pub fn address(&self) -> BleAddress {
        self.address
    }

    /// Retrieve the Received Signal Strength Indicator (RSSI) value for this
    /// advertisement, expressed in dBm. The RSSI might be the raw value or the
    /// filtered RSSI, depending on the configured signal strength filter.
    pub fn rssi(&self) -> Option<DecibelMilliwatts> {
        self.rssi
    }

    /// Retrieve the transmit power advertised by this device, if any.
    /// For BLE communication, values will range from -127 dBm to 20 dBm.
    pub fn tx_power(&self) -> Option<DecibelMilliwatts> {
        self.tx_power
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
#[derive(Clone, PartialEq, Eq, Debug)]
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::common::BleAddressKind;

    #[test]
    fn ble_advertisement_new() {
        let address = BleAddress::new(0x112233445566, BleAddressKind::Public);
        let ad = BleAdvertisement::new(address, Some(-60), Some(10));
        assert_eq!(ad.address(), address);
        assert_eq!(ad.rssi(), Some(-60));
        assert_eq!(ad.tx_power(), Some(10));
        assert!(ad.service_data_16bit_uuid.is_none());
    }

    #[test]
    fn ble_advertisement_set_and_get_service_data() {
        let address = BleAddress::new(0x112233445566, BleAddressKind::Public);
        let mut ad = BleAdvertisement::new(address, Some(-60), Some(10));

        let service_data = vec![
            ServiceData::new(0x1234, vec![0x01, 0x02, 0x03]),
            ServiceData::new(0x5678, vec![0x04, 0x05]),
        ];

        ad.set_service_data_16bit_uuid(service_data.clone());

        let retrieved_service_data = ad.service_data_16bit_uuid().unwrap();
        assert_eq!(*retrieved_service_data, service_data);
    }

    #[test]
    fn ble_advertisement_missing_service_data() {
        let address = BleAddress::new(0x112233445566, BleAddressKind::Public);
        let ad = BleAdvertisement::new(address, Some(-60), Some(10));

        let result = ad.service_data_16bit_uuid();
        assert!(result.is_err());
        assert!(matches!(
            result.unwrap_err(),
            BluetoothError::FailedPrecondition(_)
        ));
    }

    #[test]
    fn service_data_new() {
        let uuid = 0x1234;
        let data = vec![0x01, 0x02, 0x03];

        let service_data = ServiceData::new(uuid, data.clone());
        assert_eq!(service_data.uuid(), uuid);
        assert_eq!(*service_data.data(), data);
    }
}
