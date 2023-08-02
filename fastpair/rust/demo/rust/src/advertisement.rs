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

use bluetooth::{BleAddress, BleAdvertisement};

/// Holds information required to make decisions about an incoming Fast Pair
/// advertisement.
#[derive(Clone)]
pub(crate) struct FpPairingAdvertisement {
    inner: BleAdvertisement,
    /// Estimated distance in meters of device from BLE adapter.
    distance: f64,
}

impl FpPairingAdvertisement {
    /// Retrieve estimated distance the BLE advertisement travelled between
    /// the sending device and this receiver.
    pub(crate) fn distance(&self) -> f64 {
        self.distance
    }

    /// Retrieve the BLE Address of the advertising device.
    pub(crate) fn address(&self) -> BleAddress {
        self.inner.address()
    }
}

impl TryFrom<BleAdvertisement> for FpPairingAdvertisement {
    type Error = anyhow::Error;

    fn try_from(adv: BleAdvertisement) -> Result<Self, Self::Error> {
        let rssi = adv.rssi().ok_or(anyhow::anyhow!(
            "Windows advertisements should contain RSSI information."
        ))?;
        let tx_power = adv.tx_power().ok_or(anyhow::anyhow!(
            "Fast Pair advertisements should advertise their transmit power."
        ))?;

        let distance = distance_from_rssi_and_tx_power(rssi, tx_power);

        Ok(FpPairingAdvertisement {
            inner: adv,
            distance,
        })
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
