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

use crate::decoder::FpDecoder;

/// Represents a FP device model ID.
pub(crate) type ModelId = String;

/// Holds information required to make decisions about an incoming Fast Pair
/// advertisement.
#[derive(Clone)]
pub(crate) struct FpPairingAdvertisement {
    inner: BleAdvertisement,
    /// Estimated distance in meters of device from BLE adapter.
    distance: f64,
    model_id: ModelId,
}

impl FpPairingAdvertisement {
    /// Create a new Fast Pair advertisement instance.
    pub(crate) fn new(
        adv: BleAdvertisement,
        service_data: &ServiceData<u16>,
    ) -> Result<Self, anyhow::Error> {
        let rssi = adv.rssi().ok_or(anyhow::anyhow!(
            "Windows advertisements should contain RSSI information."
        ))?;
        let tx_power = adv.tx_power().ok_or(anyhow::anyhow!(
            "Fast Pair advertisements should advertise their transmit power."
        ))?;

        let distance = distance_from_rssi_and_tx_power(rssi, tx_power);

        // Extract model ID from service data. We don't need to store service
        // data in the `FpPairingAdvertisement` since it's easily accessible from
        // `FpPairingAdvertisement.inner`, but it's convenient to save the parsed
        // model ID.
        let mut model_id =
            FpDecoder::get_model_id_from_service_data(service_data).or_else(|err| {
                // Some FP advertisements can be GATT non-discoverable
                // advertisements containing service data that isn't
                // the device model ID. In this case, simply ignore
                // advertisements with errors extracting the model ID.
                // See: developers.google.com/nearby/fast-pair/specifications/service/provider
                Err(anyhow::anyhow!("error extracting model ID: {}", err))
            })?;

        if model_id.len() != 3 {
            // In this demo of Fast Pair Rust, only model ID's
            // of length 3 bytes are supported. Therefore, if a
            // larger model ID makes it this far, log an error.
            // TODO b/294453912
            return Err(anyhow::anyhow!("Error: model ID of unsupported length"));
        }

        // Pad with 0 at the beginning to successfully call `from_be_bytes`.
        // Assumes `model_id.len() == 3` before the call to `insert`, otherwise
        // this will panic.
        model_id.insert(0, 0);
        let model_id = format!("{}", u32::from_be_bytes(model_id.try_into().unwrap()));

        Ok(FpPairingAdvertisement {
            inner: adv,
            distance,
            model_id,
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
