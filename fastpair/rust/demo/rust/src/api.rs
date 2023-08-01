use std::sync::RwLock;

use bluetooth::{
    api::{BleAdapter, BleDevice, ClassicDevice},
    BleAddress, BleDataTypeId, ClassicAddress, PairingResult, Platform,
};
use flutter_rust_bridge::StreamSink;
use futures::executor;
use tracing::info;

// Sends a device name to Flutter via `StreamSink` FFI layer.
static NAME_STREAM: RwLock<Option<StreamSink<String>>> = RwLock::new(None);

// Saves the currently displayed device's address, to be used for pairing.
static CURR_ADDRESS: RwLock<Option<BleAddress>> = RwLock::new(None);

/// Sets up initial constructs and infinitely polls for advertisements.
pub fn init() {
    let run = async {
        info!("start making adapter");

        let mut adapter = Platform::default_adapter().await.unwrap();
        adapter.start_scan().unwrap();

        let datatype_selector = vec![BleDataTypeId::ServiceData16BitUuid];
        loop {
            let advertisement = adapter
                .next_advertisement(Some(&datatype_selector))
                .await
                .unwrap();

            for service_data in advertisement.service_data_16bit_uuid().unwrap() {
                let uuid = service_data.uuid();
                // This is a Fast Pair device.
                if uuid == 0x2cfe {
                    let addr = advertisement.address();
                    let ble_device = Platform::new_ble_device(addr).await.unwrap();
                    let name = ble_device.name().unwrap();
                    info!("device: {}", name);
                    if name.contains("LE_WH-1000XM3") {
                        match NAME_STREAM.read().unwrap().as_ref() {
                            Some(stream) => {
                                stream.add(name);
                                let mut curr_addr = CURR_ADDRESS.write().unwrap();
                                *curr_addr = Some(addr);
                            }
                            None => info!("Stream is None"),
                        }
                    }
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
    let result = match CURR_ADDRESS.read().unwrap().as_ref() {
        Some(addr) => {
            let run = async {
                let classic_addr = ClassicAddress::try_from(*addr).unwrap();

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
