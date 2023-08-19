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

use bluetooth::{BleAddress, BleAdvertisement, ServiceData};

use crate::{decoder::FpDecoder, error::FpError, fetcher::FpFetcher};

/// Represents a FP device model ID.
pub(crate) type ModelId = String;

/// Holds information required to make decisions about an incoming Fast Pair
/// advertisement.
#[derive(Clone, Debug)]
pub(crate) struct FpPairingAdvertisement {
    inner: BleAdvertisement,
    /// Estimated distance in meters of device from BLE adapter.
    distance: f64,
    model_id: ModelId,
    device_name: String,
    image_url: String,
}

impl FpPairingAdvertisement {
    /// Create a new Fast Pair advertisement instance.
    pub(crate) fn new(
        adv: BleAdvertisement,
        service_data: &ServiceData<u16>,
        fetcher: &Box<dyn FpFetcher>,
    ) -> Result<Self, FpError> {
        let rssi = adv.rssi().ok_or(FpError::ContractViolation(String::from(
            "Windows advertisements should contain RSSI information.",
        )))?;
        let tx_power = adv
            .tx_power()
            .ok_or(FpError::ContractViolation(String::from(
                "Windows advertisements should contain RSSI information.",
            )))?;

        let distance = distance_from_rssi_and_tx_power(rssi, tx_power);

        // Extract model ID from service data. We don't need to store service
        // data in the `FpPairingAdvertisement` since it's easily accessible from
        // `FpPairingAdvertisement.inner`, but it's convenient to save the parsed
        // model ID.
        let mut model_id = FpDecoder::get_model_id_from_service_data(service_data)?;

        if model_id.len() != 3 {
            // In this demo of Fast Pair Rust, only model ID's
            // of length 3 bytes are supported. Therefore, if a
            // larger model ID makes it this far, log an error.
            // TODO b/294453912
            return Err(FpError::Internal(String::from(
                "creating `model_id` should have already failed",
            )));
        }

        // Pad with 0 at the beginning to successfully call `from_be_bytes`.
        // Assumes `model_id.len() == 3` before the call to `insert`, otherwise
        // this will panic.
        model_id.insert(0, 0);
        let model_id = format!("{}", u32::from_be_bytes(model_id.try_into().unwrap()));

        // Retrieve device info of the device corresponding to this model ID.
        let device_info = fetcher.get_device_info_from_model_id(&model_id)?;

        Ok(FpPairingAdvertisement {
            inner: adv,
            distance,
            model_id,
            device_name: device_info.name().to_string(),
            image_url: device_info.image_url().to_string(),
        })
    }

    /// Retrieve estimated distance the BLE advertisement travelled between
    /// the sending device and this receiver.
    pub(crate) fn distance(&self) -> f64 {
        self.distance
    }

    /// Retrieve the BLE Address of the advertising device.
    pub(crate) fn address(&self) -> BleAddress {
        self.inner.address()
    }

    /// Retrieve the Model ID advertised by this device, parsed from the
    /// 16-bit UUID service data.
    pub(crate) fn model_id(&self) -> &ModelId {
        &self.model_id
    }

    pub(crate) fn device_name(&self) -> &String {
        &self.device_name
    }

    pub(crate) fn image_url(&self) -> &String {
        &self.image_url
    }
}

/// Convert RSSI and transmit power to distance using log-distance path loss
/// model, with reference path loss of 1m at 41dB in free space.
/// See: https://en.wikipedia.org/wiki/Log-distance_path_loss_model.
#[inline]
pub(crate) fn distance_from_rssi_and_tx_power(rssi: i16, tx_power: i16) -> f64 {
    // Source: Android Nearby implementation, `RangingUtils.java`.
    //
    // PL      = total path loss in db
    // txPower = TxPower in dbm
    // rssi    = Received signal strength in dbm
    // PL_0    = Path loss at reference distance d_0 {@link RSSI_DROP_OFF_AT_1_M} dbm
    // d       = length of path
    // d_0     = reference distance  (1 m)
    // gamma   = path loss exponent (2 in free space)
    //
    // Log-distance path loss (LDPL) formula:
    //
    // PL =    txPower - rssi                               = PL_0 + 10 * gamma  * log_10(d / d_0)
    //         txPower - rssi               = RSSI_DROP_OFF_AT_1_M + 10 * gamma  * log_10(d / d_0)
    //         txPower - rssi - RSSI_DROP_OFF_AT_1_M        = 10 * 2 * log_10(distanceInMeters / 1)
    //         txPower - rssi - RSSI_DROP_OFF_AT_1_M        = 20 * log_10(distanceInMeters / 1)
    //        (txPower - rssi - RSSI_DROP_OFF_AT_1_M) / 20  = log_10(distanceInMeters)
    //  10 ^ ((txPower - rssi - RSSI_DROP_OFF_AT_1_M) / 20) = distanceInMeters

    const RSSI_DROPOFF_AT_1_M: i16 = 41;
    const PATH_LOSS_EXPONENT: i16 = 2;

    f64::from(10.0).powf(
        (f64::from(tx_power - rssi - RSSI_DROPOFF_AT_1_M)) / f64::from(10 * PATH_LOSS_EXPONENT),
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fetcher::{mock::FpFetcherMock, DeviceInfo};

    use bluetooth::BleAddressKind;

    #[test]
    fn test_new_fp_pairing_advertisement() {
        let addr = BleAddress::new(0x112233, BleAddressKind::Public);
        let ble_adv = BleAdvertisement::new(addr, Some(-60), Some(10));

        let raw_data = vec![3, 2, 1];
        let expected_model_id = "197121"; // (3 << 16) + (2 << 8) + 1.
        let service_data = ServiceData::new(0x123 as u16, raw_data);

        let image_url = String::from("image_url");
        let device_name = String::from("name");
        let device_info = Ok(DeviceInfo::new(image_url.clone(), device_name.clone()));
        let fetcher: Box<dyn FpFetcher> = Box::new(FpFetcherMock::new(device_info));

        let fp_adv = FpPairingAdvertisement::new(ble_adv, &service_data, &fetcher);

        assert!(fp_adv.is_ok());
        let fp_adv = fp_adv.unwrap();
        assert_eq!(fp_adv.address(), addr);
        assert_eq!(fp_adv.image_url(), &image_url);
        assert_eq!(fp_adv.device_name(), &device_name);
        assert_eq!(fp_adv.model_id(), &expected_model_id);
    }

    #[test]
    fn test_new_fp_pairing_advertisement_bad_rssi() {
        let addr = BleAddress::new(0x112233, BleAddressKind::Public);
        let ble_adv = BleAdvertisement::new(addr, None, Some(10));

        let raw_data = vec![3, 2, 1];
        let service_data = ServiceData::new(0x123 as u16, raw_data);

        let device_info = Ok(DeviceInfo::new(
            String::from("image_url"),
            String::from("name"),
        ));
        let fetcher: Box<dyn FpFetcher> = Box::new(FpFetcherMock::new(device_info));

        let fp_adv = FpPairingAdvertisement::new(ble_adv, &service_data, &fetcher);

        assert!(fp_adv.is_err());
        assert!(matches!(fp_adv.unwrap_err(), FpError::ContractViolation(_)));
    }

    #[test]
    fn test_new_fp_pairing_advertisement_bad_tx_power() {
        let addr = BleAddress::new(0x112233, BleAddressKind::Public);
        let ble_adv = BleAdvertisement::new(addr, Some(-60), None);

        let raw_data = vec![3, 2, 1];
        let service_data = ServiceData::new(0x123 as u16, raw_data);

        let device_info = Ok(DeviceInfo::new(
            String::from("image_url"),
            String::from("name"),
        ));
        let fetcher: Box<dyn FpFetcher> = Box::new(FpFetcherMock::new(device_info));

        let fp_adv = FpPairingAdvertisement::new(ble_adv, &service_data, &fetcher);

        assert!(fp_adv.is_err());
        assert!(matches!(fp_adv.unwrap_err(), FpError::ContractViolation(_)));
    }

    #[test]
    fn test_new_fp_pairing_advertisement_bad_fetcher() {
        let addr = BleAddress::new(0x112233, BleAddressKind::Public);
        let ble_adv = BleAdvertisement::new(addr, Some(-60), Some(10));

        let raw_data = vec![3, 2, 1];
        let service_data = ServiceData::new(0x123 as u16, raw_data);

        let fetcher: Box<dyn FpFetcher> = Box::new(FpFetcherMock::new(Err(FpError::Test)));

        let fp_adv = FpPairingAdvertisement::new(ble_adv, &service_data, &fetcher);

        assert!(fp_adv.is_err());
        assert!(matches!(fp_adv.unwrap_err(), FpError::Test));
    }
}
