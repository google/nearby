use std::sync::RwLock;

use bluetooth::{
    api::{BleAdapter, BleDevice},
    BleDataTypeId, Platform,
};
use flutter_rust_bridge::StreamSink;
use futures::executor;
use tracing::info;

// Sends a device name to Flutter via `StreamSink` FFI layer.
static NAME_STREAM: RwLock<Option<StreamSink<String>>> = RwLock::new(None);

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

                    match NAME_STREAM.read().unwrap().as_ref() {
                        Some(s) => {
                            s.add(name);
                        }
                        None => info!("Stream is None"),
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
