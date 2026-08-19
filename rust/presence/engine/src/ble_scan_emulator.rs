// Defines FFI interfaces to access BLE Scan system APIs.
// The codes will be reimplemented in C.
// Two-way communication between Rust and a hosting language are implemented by
// two simple functions.
//   1. start_scan() implemented by the hosting language is called from Rust to
//      pass in scan parameters.
//   2. on_scan_data() implemented by Rust is called by the hosting language to pass
//      scan data to Rust codes.
use std::thread;
use std::thread::sleep;
use std::time::Duration;
use log::info;
use crate::ble_scan_provider;
use crate::scan_controller::ScanData;

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