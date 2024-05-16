// Construct data structures passed from Rust to C.
include!(concat!(env!("OUT_DIR"), "/rust_to_c_autogen.rs"));

use presence_core::ble_scan_provider::ScanRequest;
use presence_core::client_provider::{DiscoveryResult, PresenceMedium};

// Convert ScanRequest to *PresenceBleScanRequest, which can be parsed by C codes.
impl PresenceBleScanRequest {
    pub fn from_scan_request(scan_request: ScanRequest) -> *mut Self {
        unsafe {
            let ble_scan_request = presence_ble_scan_request_new(scan_request.priority);
            for action in scan_request.actions {
                presence_ble_scan_request_add_action(ble_scan_request, action);
            }
            ble_scan_request
        }
    }
}

// Convert DiscoveryResult to *PresenceDiscoveryResult, which can be parsed by C codes.
impl PresenceDiscoveryResult {
    pub fn from_discovery_result(result: DiscoveryResult) -> *mut Self {
        unsafe {
            let presence_result = presence_discovery_result_new(result.medium);
            for action in result.device.actions {
                presence_discovery_result_add_action(presence_result, action);
            }
            presence_result
        }
    }
}
