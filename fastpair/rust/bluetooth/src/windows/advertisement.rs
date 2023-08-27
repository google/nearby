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

use windows::{
    // Struct that receives Bluetooth Low Energy (LE) advertisements.
    // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcher?view=winrt-22621
    Devices::Bluetooth::Advertisement::{
        BluetoothLEAdvertisementDataSection,
        BluetoothLEAdvertisementReceivedEventArgs,
    },

    // Struct representing an immutable view into a vector.
    // https://learn.microsoft.com/en-us/uwp/api/windows.foundation.collections.ivectorview-1?view=winrt-22621
    Foundation::Collections::IVectorView,

    // Struct for reading data from a Windows stream, like an IVectorView.
    // https://learn.microsoft.com/en-us/uwp/api/windows.storage.streams.datareader?view=winrt-22621
    Storage::Streams::DataReader,
};

use crate::common::{
    BleAddress, BleAddressKind, BleAdvertisement, BleDataTypeId,
    BluetoothError, ServiceData,
};

impl TryFrom<&BluetoothLEAdvertisementReceivedEventArgs> for BleAdvertisement {
    type Error = BluetoothError;

    fn try_from(
        adv: &BluetoothLEAdvertisementReceivedEventArgs,
    ) -> Result<Self, Self::Error> {
        let addr = adv.BluetoothAddress()?;
        let kind = BleAddressKind::try_from(adv.BluetoothAddressType()?)?;

        let addr = BleAddress::new(addr, kind);
        // `rssi` and tx_power` aren't always advertised, so convert to None if
        // can't extract value.
        let rssi = adv.RawSignalStrengthInDBm().ok();
        let tx_power = match adv.TransmitPowerLevelInDBm() {
            Ok(val_ref) => val_ref.GetInt16().ok(),
            Err(_) => None,
        };

        Ok(BleAdvertisement::new(addr, rssi, tx_power))
    }
}

impl BleAdvertisement {
    /// Load data of selected data types into self by parsing the raw Windows
    /// advertisement.
    /// See: Supplement to the Bluetooth Core Specification Part A, Section 1.
    pub(crate) fn load_data(
        &mut self,
        adv: &BluetoothLEAdvertisementReceivedEventArgs,
        datatype_ids: &[BleDataTypeId],
    ) -> Result<(), BluetoothError> {
        let adv = adv.Advertisement()?;

        for datatype_id in datatype_ids {
            // Note `raw_data_sections` is `!Send` and `!Sync`. This means
            // processing must occur in a synchronous environment. The compiler
            // will complain if parsing is done in an async function.
            let raw_data_sections =
                adv.GetSectionsByType((*datatype_id) as u8)?;
            match datatype_id {
                BleDataTypeId::ServiceData16BitUuid => {
                    let service_data =
                        parse_service_data_16bit_uuid(raw_data_sections)?;
                    self.set_service_data_16bit_uuid(service_data)
                }
            };
        }

        Ok(())
    }
}

/// Parse the advertisement's service data.
/// Further Reading:
/// * `BleMedium::AdvertisementReceivedHandler` under
/// github.com/google/nearby/internal/platform/implementation/windows_ble/ble_medium.cc.
/// * Bluetooth Supplement to the Core Specification, Part A, Section 1.11.
/// * go/fast_pair_windows_data_parse.
#[inline]
fn parse_service_data_16bit_uuid(
    raw_data_sections: IVectorView<BluetoothLEAdvertisementDataSection>,
) -> Result<Vec<ServiceData<u16>>, BluetoothError> {
    let mut data_vec = Vec::new();

    for raw_data in raw_data_sections {
        let data_reader = DataReader::FromBuffer(&raw_data.Data()?)?;
        let uuid = data_reader.ReadUInt16()?;

        let unconsumed_buffer_len =
            data_reader.UnconsumedBufferLength()? as usize;

        let mut data = vec![0u8; unconsumed_buffer_len];
        data_reader.ReadBytes(&mut data)?;

        data_vec.push(ServiceData::new(uuid, data));
    }

    Ok(data_vec)
}
