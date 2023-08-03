use std::{collections::HashMap, sync::RwLock};

use bluetooth::{
    api::{BleAdapter, BleDevice, ClassicDevice},
    BleAdvertisement, BleDataTypeId, ClassicAddress, PairingResult, Platform, ServiceData,
};
use flutter_rust_bridge::StreamSink;
use futures::executor;
use tracing::{info, warn};

use crate::advertisement::FpPairingAdvertisement;

// Sends a device name to Flutter via `StreamSink` FFI layer.
static NAME_STREAM: RwLock<Option<StreamSink<String>>> = RwLock::new(None);

// Saves the currently displayed device's advertisement, to be used for pairing.
static CURR_DEVICE_ADV: RwLock<Option<FpPairingAdvertisement>> = RwLock::new(None);

/// Updates the device name as displayed by Flutter.
#[inline]
async fn update_best_device(best_adv: FpPairingAdvertisement) {
    let addr = best_adv.address();
    let ble_device = Platform::new_ble_device(addr).await.unwrap();
    let name = ble_device.name().unwrap();

    match NAME_STREAM.read().unwrap().as_ref() {
        Some(stream) => {
            stream.add(name);
        }
        None => info!("Stream is None"),
    }
    let mut curr_adv = CURR_DEVICE_ADV.write().unwrap();
    *curr_adv = Some(best_adv);
}

/// Determines whether the device advertised by the provided service data is the
/// closest Fast Pair device.
/// If this device has been seen previously but has now moved further away,
/// decide which other seen device is now closer.
#[inline]
fn new_best_fp_advertisement(
    advertisement: BleAdvertisement,
    service_data: &ServiceData<u16>,
    latest_advertisement_map: &mut HashMap<String, FpPairingAdvertisement>,
) -> Option<FpPairingAdvertisement> {
    // Analyze service data sections.
    let uuid = service_data.uuid();

    // This is not a Fast Pair device.
    if uuid != 0x2cfe {
        return None;
    }

    let fp_adv = match FpPairingAdvertisement::new(advertisement, service_data) {
        Ok(fp_adv) => fp_adv,
        Err(err) => {
            // If error during construction (e.g. non-discoverable
            // Fast Pair device not advertising tx_power or
            // with service data that isn't model ID) ignore
            // this advertisement section.
            warn!("Error creating FP Advertisement: {}", err);
            return None;
        }
    };

    latest_advertisement_map.insert(fp_adv.model_id().to_owned(), fp_adv.clone());

    if let Some(best_adv) = CURR_DEVICE_ADV.read().unwrap().as_ref() {
        if best_adv.distance() >= fp_adv.distance() {
            // New advertised distance is closer.
            Some(fp_adv)
        } else if best_adv.model_id() == fp_adv.model_id() {
            // New advertised distance by the previous best device has
            // increased, so select new closest device.
            let next_best_adv_ref =
                latest_advertisement_map
                    .values()
                    .into_iter()
                    .min_by(|adv1, adv2| {
                        // We should never get NaN, so it's okay to unwrap.
                        adv1.distance().partial_cmp(&adv2.distance()).unwrap()
                    });

            if let Some(next_best_adv) = next_best_adv_ref {
                Some(next_best_adv.to_owned())
            } else {
                None
            }
        } else {
            None
        }
    } else {
        // First discovered device, must be closest.
        Some(fp_adv)
    }
}

/// Sets up initial constructs and infinitely polls for advertisements.
pub fn init() {
    let run = async {
        info!("start making adapter");

        let mut adapter = Platform::default_adapter().await.unwrap();
        adapter.start_scan().unwrap();

        let mut latest_advertisement_map = HashMap::new();
        let datatype_selector = vec![BleDataTypeId::ServiceData16BitUuid];

        loop {
            // Retrieve the next received advertisement.
            let advertisement = adapter
                .next_advertisement(Some(&datatype_selector))
                .await
                .unwrap();

            for service_data in advertisement.service_data_16bit_uuid().unwrap() {
                if let Some(best_adv) = new_best_fp_advertisement(
                    advertisement.clone(),
                    service_data,
                    &mut latest_advertisement_map,
                ) {
                    update_best_device(best_adv).await;
                }
            }
        }
    };

    executor::block_on(run)
}

/// Sets up `StreamSink` for Dart-Rust FFI.
pub fn event_stream(s: StreamSink<String>) -> Result<(), anyhow::Error> {
    let mut stream = NAME_STREAM.write().unwrap();
    *stream = Some(s);
    Ok(())
}

/// Attempt classic pairing with device of address `CURR_ADDRESS`.
pub fn pair() -> String {
    let result = match CURR_DEVICE_ADV.read().unwrap().as_ref() {
        Some(adv) => {
            let run = async {
                let classic_addr = ClassicAddress::try_from(adv.address()).unwrap();

                let classic_device = Platform::new_classic_device(classic_addr).await.unwrap();

                match classic_device.pair().await {
                    Ok(result) => match result {
                        PairingResult::Success => String::from("Pairing success!"),
                        PairingResult::AlreadyPaired => {
                            String::from("This device is already paired.")
                        }
                        PairingResult::AlreadyInProgress => {
                            String::from("Pairing already in progress.")
                        }
                        _ => String::from("Unknown result."),
                    },
                    Err(err) => {
                        format!("Error {}", err)
                    }
                }
            };

            executor::block_on(run)
        }
        None => String::from("No device available to pair."),
    };
    info!(result);
    result
}
