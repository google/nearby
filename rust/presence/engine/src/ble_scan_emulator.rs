use std::thread;
use std::thread::sleep;
use std::time::Duration;
use log::info;
use tokio::sync::mpsc;
use crate::{ble_scan_provider, client_provider, provider};
use crate::scan_controller::{ScanDataMsg, ScanData};
use crate::fused_engine::ProviderEvent;

pub struct BleScanRequest {}
pub fn start_scan(request: BleScanRequest) {
    info!("Mock BLE scanner starts");
    std::thread::spawn(move || {
            for i in 1..4 {
                sleep(Duration::new(1, 0));
                info!("BLE scanner received advertisement: {}.", i);
                ble_scan_provider::on_scan_data(ScanData{action: 1});
            }
    });
}