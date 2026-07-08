use crate::rust_to_c::PresenceBleScanRequest;
use log::debug;
use presence_core::ble_scan_provider::{BleScanner, ScanRequest};

pub type PresenceStartBleScan = extern "C" fn(*mut PresenceBleScanRequest);
pub struct BleScannerCpp {
    pub(crate) presence_start_ble_scan: PresenceStartBleScan,
}

impl BleScanner for BleScannerCpp {
    fn start_ble_scan(&self, request: ScanRequest) {
        debug!("BleScanner start ble scan with request {:?}.", request);
        (self.presence_start_ble_scan)(PresenceBleScanRequest::from_scan_request(request));
    }
}
