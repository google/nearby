use std::thread;
use std::thread::sleep;
use std::time::Duration;
use log::info;
use tokio::sync::mpsc;
use crate::{ble_scan_provider, client_provider, provider};
use crate::scan_controller::{ScanDataMsg, ScanData};
use crate::fused_engine::ProviderEvent;

static mut PROVIDER_TX: Option<mpsc::Sender<ProviderEvent>> = None;

pub struct BleScanRequest {}
pub fn start_scan(request: BleScanRequest) {
    info!("Mock BLE scanner starts");
    std::thread::spawn(move || {
        unsafe {
            for i in 1..4 {
                sleep(Duration::new(1, 0));
                info!("======================================");
                info!("BLE scanner received advertisement: {}.", i);
                provider::update_engine(PROVIDER_TX.as_ref().unwrap().clone(),
                                        ProviderEvent::ScanDataMsg(ScanDataMsg::ScanData(ScanData {action: 1})));
            }
        }
    });
}

// FFI called by BLE scan Provider to register the communication channel into the Provider,
// which can use the channel to send scan results later.
pub fn register_provider_tx(provider_tx: mpsc::Sender<ProviderEvent>) {
    unsafe {
        PROVIDER_TX = Some(provider_tx);
    }
}