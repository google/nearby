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

use super::BluetoothError;

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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ble_address_new() {
        let addr = BleAddress::new(0x112233445566, BleAddressKind::Public);
        assert_eq!(addr.val, [0x66, 0x55, 0x44, 0x33, 0x22, 0x11]);
        assert_eq!(addr.kind, BleAddressKind::Public);
    }

    #[test]
    fn ble_address_get_kind() {
        let addr_public =
            BleAddress::new(0x112233445566, BleAddressKind::Public);
        assert_eq!(addr_public.get_kind(), BleAddressKind::Public);

        let addr_random =
            BleAddress::new(0xAABBCCDDEEFF, BleAddressKind::Random);
        assert_eq!(addr_random.get_kind(), BleAddressKind::Random);
    }

    #[test]
    fn ble_address_into_u64() {
        let ble_addr = BleAddress::new(0x112233445566, BleAddressKind::Public);
        let u64_addr: u64 = ble_addr.into();
        assert_eq!(u64_addr, 0x112233445566);
    }

    #[test]
    fn classic_address_from_u64() {
        let u64_addr = 0x112233445566;
        let classic_addr: ClassicAddress = u64_addr.into();
        assert_eq!(classic_addr.0, [0x66, 0x55, 0x44, 0x33, 0x22, 0x11]);
    }

    #[test]
    fn try_from_ble_address_to_classic() {
        let ble_addr = BleAddress::new(0x112233445566, BleAddressKind::Public);
        let result: Result<ClassicAddress, BluetoothError> =
            ble_addr.try_into();
        assert!(result.is_ok());
        assert_eq!(result.unwrap().0, [0x66, 0x55, 0x44, 0x33, 0x22, 0x11]);

        let ble_addr_random =
            BleAddress::new(0xAABBCCDDEEFF, BleAddressKind::Random);
        let result: Result<ClassicAddress, BluetoothError> =
            TryFrom::try_from(ble_addr_random);
        assert!(result.is_err());
        assert!(matches!(
            result.unwrap_err(),
            BluetoothError::BadTypeConversion(_),
        ));
    }

    #[test]
    fn test_u64_to_6lsb() {
        // Test a case where the input number is smaller than 6 bytes
        let num = 0x123456;
        let expected_result = [0x56, 0x34, 0x12, 0, 0, 0];
        let result = u64_to_6lsb(num);
        assert_eq!(result, expected_result);

        // Test a case where the input number is exactly 6 bytes
        let num = 0xAABBCCDDEEFF;
        let expected_result = [0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA];
        let result = u64_to_6lsb(num);
        assert_eq!(result, expected_result);

        // Test a case where the number is too large so the two most significant
        // bytes get dropped.
        let num = 0x1122334455667788;
        let expected_result = [0x88, 0x77, 0x66, 0x55, 0x44, 0x33];
        let result = u64_to_6lsb(num);
        assert_eq!(result, expected_result);

        // Test a case where the input number is 0
        let num = 0;
        let expected_result = [0, 0, 0, 0, 0, 0];
        let result = u64_to_6lsb(num);
        assert_eq!(result, expected_result);
    }
}
