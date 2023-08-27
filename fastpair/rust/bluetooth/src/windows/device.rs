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

use async_trait::async_trait;
use tracing::{info, warn};
use windows::{
    Devices::{
        Bluetooth::{
            // Tuple struct describing the type of address (public, random, unspecified).
            // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothaddresstype?view=winrt-22621
            BluetoothAddressType,
            
            // Struct for interacting with a discovered BT Classic device.
            // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothdevice?view=winrt-22621
            BluetoothDevice,
            
            // Struct for interacting with a discovered BLE device.
            // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothledevice?view=winrt-22621
            BluetoothLEDevice,
        },
        Enumeration::{
            // Struct for custom pairing with a device.
            // https://learn.microsoft.com/en-us/uwp/api/windows.devices.enumeration.deviceinformationcustompairing?view=winrt-22621
            DeviceInformationCustomPairing,
            
            // Tuple struct to indicate the kinds of pairing supported by the application.
            // https://learn.microsoft.com/en-us/uwp/api/windows.devices.enumeration.devicepairingkinds?view=winrt-22621
            DevicePairingKinds,
            
            // Struct for retrieving data about a PairingRequested event.
            // https://learn.microsoft.com/en-us/uwp/api/windows.devices.enumeration.devicepairingrequestedeventargs?view=winrt-22621
            DevicePairingRequestedEventArgs,
        },
    },
    // Wraps a closure for handling events associated with a struct
    // (e.g. PairingRequested event in `DeviceInformationCustomPairing`).
    // https://learn.microsoft.com/en-us/uwp/api/windows.foundation.typedeventhandler-2?view=winrt-22621
    Foundation::TypedEventHandler,
};

use crate::{api, common::{BleAddress, ClassicAddress, BluetoothError, PairingResult}};

/// Concrete type implementing `Device`, used for Windows BLE.
pub struct BleDevice {
    inner: BluetoothLEDevice,
    addr: BleAddress,
}

/// Concrete type implementing `Device`, used for Windows Bluetooth Classic.
pub struct ClassicDevice {
    inner: BluetoothDevice,
    addr: ClassicAddress,
}

#[async_trait]
impl api::BleDevice for BleDevice {
    async fn new(addr: BleAddress) -> Result<Self, BluetoothError> {
        let kind = BluetoothAddressType::from(addr.get_kind());
        let raw_addr = u64::from(addr);

        let inner = BluetoothLEDevice::FromBluetoothAddressWithBluetoothAddressTypeAsync(
            raw_addr, kind,
        )?
        .await?;

        Ok(BleDevice { inner, addr })
    }

    fn name(&self) -> Result<String, BluetoothError> {
        Ok(self.inner.Name()?.to_string())
    }

    fn address(&self) -> BleAddress {
        self.addr
    }
}

#[async_trait]
impl api::ClassicDevice for ClassicDevice {
    async fn new(addr: ClassicAddress) -> Result<Self, BluetoothError> {
        let raw_addr = u64::from(addr);

        let inner = BluetoothDevice::FromBluetoothAddressAsync(
            raw_addr,
        )?
        .await?;

        Ok(ClassicDevice { inner, addr })
    }

    fn name(&self) -> Result<String, BluetoothError> {
        Ok(self.inner.Name()?.to_string_lossy())
    }

    fn address(&self) -> ClassicAddress {
        self.addr
    }

    async fn pair(&self) -> Result<PairingResult, BluetoothError> {
        let pair_info = self.inner.DeviceInformation()?.Pairing()?;
        if pair_info.IsPaired()? {
            info!("Device already paired");
            Ok(PairingResult::AlreadyPaired)
        } else if !pair_info.CanPair()? {
            info!("Device can't pair");
            Err(BluetoothError::PairingFailed(String::from("device can't pair")))
        } else {  
            let custom = pair_info.Custom()?;
            custom.PairingRequested(&TypedEventHandler::new(
                |_custom: &Option<DeviceInformationCustomPairing>, 
                event_args: &Option<DevicePairingRequestedEventArgs>,
                |  {
                    if let Some(event_args) = event_args {
                        match event_args.PairingKind()? {
                            DevicePairingKinds::ConfirmOnly => {
                                event_args.Accept()                            
                            }
                            _ => {
                                warn!("Unsupported pairing kind {:?}", event_args.PairingKind());
                                Ok(())
                            }
                        }
                    } else {
                        warn!("Empty pairing event arguments");
                        Ok(())
                    }

                },
            ))?;
            let res = custom
                .PairAsync(
                    DevicePairingKinds::ConfirmOnly
                        | DevicePairingKinds::ProvidePin
                        | DevicePairingKinds::ConfirmPinMatch
                        | DevicePairingKinds::DisplayPin,
                )?
                .await?;
            let status = PairingResult::from(res.Status()?);

            match status {
                PairingResult::Failure(msg) => Err(BluetoothError::PairingFailed(msg)),
                _ => Ok(status),
            }
        }
    }
}

mod tests {
    // TODO b/288592509 unit tests
}
