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

use std::{collections::HashMap, sync::RwLock, time::Duration};

use bluetooth::{
    api::{BleAdapter, ClassicDevice},
    BleAdvertisement, BleDataTypeId, ClassicAddress, PairingResult, Platform, ServiceData,
};
use flutter_rust_bridge::StreamSink;
use futures::executor;
use tracing::{info, warn};
use ttl_cache::TtlCache;

use crate::{
    advertisement::{FpPairingAdvertisement, ModelId},
    fetcher::{FpFetcher, FpFetcherFs},
};

// Sends a device name to Flutter via `StreamSink` FFI layer.
static DEVICE_STREAM: RwLock<Option<StreamSink<Option<[String; 2]>>>> = RwLock::new(None);

// Saves the currently displayed device's advertisement, to be used for pairing.
static CURR_DEVICE_ADV: RwLock<Option<FpPairingAdvertisement>> = RwLock::new(None);

// Temporarily restricts which model IDs can be displayed.
static MODEL_ID_BLACKLIST: RwLock<Option<TtlCache<ModelId, ()>>> = RwLock::new(None);

// Specifies how long entries should blacklisted for.
const TTL_BLACKLIST: Duration = Duration::from_secs(10);

/// Updates the device name as displayed by Flutter.
#[inline]
async fn update_best_device(best_adv: FpPairingAdvertisement) {
    match DEVICE_STREAM.read().unwrap().as_ref() {
        Some(stream) => {
            stream.add(Some([
                best_adv.device_name().to_string(),
                best_adv.image_url().to_string(),
            ]));
        }
        None => info!("Name stream is None"),
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
    fetcher: &Box<dyn FpFetcher>,
    latest_advertisement_map: &mut HashMap<String, FpPairingAdvertisement>,
) -> Option<FpPairingAdvertisement> {
    // Analyze service data sections.
    let uuid = service_data.uuid();

    // This is not a Fast Pair device.
    if uuid != 0x2cfe {
        return None;
    }

    let fp_adv = match FpPairingAdvertisement::new(advertisement, service_data, fetcher) {
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

    // If blacklisted in TTL cache, skip this advertisement.
    let blacklisted = match MODEL_ID_BLACKLIST.read().unwrap().as_ref() {
        Some(cache) => cache.get(fp_adv.model_id()).is_some(),
        None => false,
    };
    if blacklisted {
        latest_advertisement_map.remove(fp_adv.model_id());
        return None;
    }

    // Else, this advertisement is now the most recent advertisement by the
    // device with model ID `fp_adv.model_id()`.
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

            // If next closest device exists, it's now the closest!
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

/// Sets up necessary constructs to maintain a TTL blacklist of model IDs.
#[inline]
fn init_cache() {
    let mut cache = MODEL_ID_BLACKLIST.write().unwrap();
    *cache = Some(TtlCache::new(16));
}

/// Sets up initial constructs and infinitely polls for advertisements.
pub fn init() {
    const JSON_PATH: &str = "./local";

    let run = async {
        info!("start making adapter");

        let mut adapter = Platform::default_adapter().await.unwrap();
        adapter.start_scan().unwrap();

        init_cache();

        let mut latest_advertisement_map = HashMap::new();
        let datatype_selector = vec![BleDataTypeId::ServiceData16BitUuid];
        let fetcher: Box<dyn FpFetcher> = Box::new(FpFetcherFs::new(String::from(JSON_PATH)));

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
                    &fetcher,
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
pub fn event_stream(s: StreamSink<Option<[String; 2]>>) {
    let mut stream = DEVICE_STREAM.write().unwrap();
    *stream = Some(s);
}

/// Attempt classic pairing with currently displayed device.
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

/// Remove this device from display and add it to the TTL cache blacklist.
pub fn dismiss() {
    let run = async {
        let mut adv = CURR_DEVICE_ADV.write().unwrap();
        match MODEL_ID_BLACKLIST.write().unwrap().as_mut() {
            Some(cache) => {
                // Get rid of the best (i.e. currently displayed) device.
                let adv = adv.take();
                match adv {
                    Some(adv) => {
                        // Add device to TTL blacklist.
                        cache.insert(adv.model_id().to_string(), (), TTL_BLACKLIST);
                    }
                    None => (),
                }

                // Ensure the currently-displayed device is no longer displayed.
                match DEVICE_STREAM.read().unwrap().as_ref() {
                    Some(stream) => {
                        stream.add(None);
                    }
                    None => (),
                }
            }
            None => (),
        }
    };

    executor::block_on(run);
}
