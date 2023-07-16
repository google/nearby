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

use crate::bluetooth::common::BluetoothError;

/// BLE Addresses can either be the peripheral's public MAC address, or various
/// types of random addresses.
#[derive(PartialEq, Eq, Clone, Copy, Debug, Hash)]
pub enum BleAddressKind {
    Public,
    Random,
}

/// Struct representing a 48-bit BLE Address and its type.
#[derive(PartialEq, Eq, Clone, Copy, Debug, Hash)]
pub struct BleAddress {
    val: [u8; 6],
    kind: BleAddressKind,
}

/// Struct representing a 48-bit BT Classic address.
#[derive(PartialEq, Eq, Clone, Copy, Debug, Hash)]
pub struct ClassicAddress([u8; 6]);

/// Enum for interfacing with Bluetooth Addresses.
#[derive(PartialEq, Eq, Clone, Copy, Debug, Hash)]
pub enum Address {
    Ble(BleAddress),
    Classic(ClassicAddress),
}

impl BleAddress {
    /// `BleAddress` constructor.
    pub fn new(addr: u64, kind: BleAddressKind) -> Self {
        let addr = u64_to_6lsb(addr);

        BleAddress { val: addr, kind }
    }

    /// Retrieve the type of BLE Address (public or random).
    pub fn get_kind(&self) -> BleAddressKind {
        self.kind
    }
}

/// Function for converting the six LSB of a u64 into a 6-byte array.
#[inline]
fn u64_to_6lsb(num: u64) -> [u8; 6] {
    num.to_le_bytes()[..6]
        .try_into()
        .expect("Sanity check, slice length matches array length")
}

impl From<u64> for ClassicAddress {
    fn from(addr: u64) -> Self {
        let addr = u64_to_6lsb(addr);

        ClassicAddress(addr)
    }
}

impl TryFrom<BleAddress> for ClassicAddress {
    type Error = BluetoothError;

    fn try_from(addr: BleAddress) -> Result<Self, Self::Error> {
        match addr.kind {
            BleAddressKind::Public => Ok(ClassicAddress(addr.val)),
            BleAddressKind::Random => Err(BluetoothError::BadTypeConversion(String::from(
                "can't convert BLE Random address to Bluetooth Classic address."
            ))),
        }
    }
}

impl From<BleAddress> for u64 {
    fn from(addr: BleAddress) -> Self {
        let mut bytes = [0u8; 8];
        bytes[..6].copy_from_slice(&addr.val);

        u64::from_le_bytes(bytes)
    }
}

impl From<ClassicAddress> for u64 {
    fn from(addr: ClassicAddress) -> Self {
        let mut bytes = [0u8; 8];
        bytes[..6].copy_from_slice(&addr.0);

        u64::from_le_bytes(bytes)
    }
}
