include!(concat!(env!("OUT_DIR"), "/cpp_ffi.rs"));

use log::{debug, info};
use tokio::sync::mpsc;

use presence_core::ble_scan_provider::{BleScanProvider, BleScanner, PresenceBleScanResult};
use presence_core::client_provider::{DiscoveryCallback, PresenceClientProvider};

use presence_core::{DiscoveryResult, PresenceEngine, ProviderEvent};
pub use presence_core::{
    PresenceDiscoveryCondition, PresenceDiscoveryRequest, PresenceIdentityType,
    PresenceMeasurementAccuracy, PresenceMedium,
};

pub struct PresenceDiscoveryRequestBuilder {
    priority: i32,
    conditions: Vec<PresenceDiscoveryCondition>,
}

impl PresenceDiscoveryRequestBuilder {
    pub fn new(priority: i32) -> Self {
        Self {
            priority,
            conditions: Vec::new(),
        }
    }

    pub fn add_condition(&mut self, condition: PresenceDiscoveryCondition) {
        self.conditions.push(condition);
    }

    pub fn build(&self) -> PresenceDiscoveryRequest {
        PresenceDiscoveryRequest {
            priority: self.priority,
            conditions: self.conditions.to_vec(),
        }
    }
}

struct PresenceBleScanResultBuilder {
    pub priority: i32,
    actions: Vec<i32>,
}

impl PresenceBleScanResultBuilder {
    pub fn new(priority: i32) -> Self {
        Self {
            priority,
            actions: Vec::new(),
        }
    }

    pub fn add_action(&mut self, action: i32) {
        self.actions.push(action);
    }

    pub fn build(&self) -> PresenceBleScanResult {
        PresenceBleScanResult {
            priority: self.priority,
            actions: self.actions.to_vec(),
        }
    }
}

pub type PresenceDiscoveryCallback = fn(*mut PresenceDiscoveryResult);
struct DiscoveryCallbackCpp {
    presence_discovery_callback: PresenceDiscoveryCallback,
}

impl DiscoveryCallback for DiscoveryCallbackCpp {
    fn on_device_updated(&self, result: DiscoveryResult) {
        unsafe {
            let presence_result = presence_discovery_result_new(PresenceMedium::Unknown);
            for action in result.actions {
                presence_discovery_result_add_action(presence_result, action);
            }
            (self.presence_discovery_callback)(presence_result);
        }
    }
}

pub type PresenceStartBleScan = fn(*mut PresenceBleScanRequest);
struct BleScannerCpp {
    presence_start_ble_scan: PresenceStartBleScan,
}

impl BleScanner for BleScannerCpp {
    fn start_ble_scan(&self, request: PresenceDiscoveryRequest) {
        info!("BleScanner start ble scan with request {:?}.", request);
        unsafe {
            let scan_request = presence_ble_scan_request_new(request.priority);
            for condition in request.conditions {
                presence_ble_scan_request_add_action(scan_request, condition.action);
            }
            (self.presence_start_ble_scan)(scan_request);
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn presence_engine_new(
    presence_discovery_callback: PresenceDiscoveryCallback,
    presence_start_ble_scan: PresenceStartBleScan,
) -> *mut PresenceEngine {
    env_logger::init();
    info!("presence_engine_new.");
    // Channel for Providers to send events to Engine.
    let (provider_event_tx, provider_event_rx) = mpsc::channel::<ProviderEvent>(100);
    let engine_ptr = Box::into_raw(Box::new(PresenceEngine::new(
        provider_event_tx,
        provider_event_rx,
        Box::new(DiscoveryCallbackCpp {
            presence_discovery_callback,
        }),
        Box::new(BleScannerCpp {
            presence_start_ble_scan,
        }),
    )));
    engine_ptr
}

#[no_mangle]
pub unsafe extern "C" fn presence_engine_run(engine: *mut PresenceEngine) {
    println!("start engine.");
    (*engine).run();
}
#[no_mangle]
pub unsafe extern "C" fn presence_engine_set_request(
    engine: *mut PresenceEngine,
    request: *mut PresenceDiscoveryRequest,
) {
    (*engine)
        .get_client_provider()
        .set_request(*Box::from_raw(request));
}

#[no_mangle]
pub unsafe extern "C" fn presence_ble_scan_callback(
    engine: *mut PresenceEngine,
    scan_result: *mut PresenceBleScanResult,
) {
    info!("ble_scan_callback");
    (*engine)
        .get_ble_scan_provider()
        .on_scan_result(*(Box::from_raw(scan_result)));
}

#[no_mangle]
pub extern "C" fn presence_request_builder_new(
    priority: i32,
) -> *mut PresenceDiscoveryRequestBuilder {
    Box::into_raw(Box::new(PresenceDiscoveryRequestBuilder::new(priority)))
}

#[no_mangle]
pub unsafe extern "C" fn presence_request_builder_add_condition(
    builder: *mut PresenceDiscoveryRequestBuilder,
    action: i32,
    identity_type: PresenceIdentityType,
    measurement_accuracy: PresenceMeasurementAccuracy,
) {
    (*builder).add_condition(PresenceDiscoveryCondition {
        action,
        identity_type,
        measurement_accuracy,
    });
}

#[no_mangle]
pub unsafe extern "C" fn presence_request_builder_build(
    builder: *mut PresenceDiscoveryRequestBuilder,
) -> *mut PresenceDiscoveryRequest {
    Box::into_raw(Box::new(Box::from_raw(builder).build()))
}

#[no_mangle]
pub extern "C" fn presence_ble_scan_result_builder_new(
    priority: i32,
) -> *mut PresenceBleScanResultBuilder {
    Box::into_raw(Box::new(PresenceBleScanResultBuilder::new(priority)))
}

#[no_mangle]
pub unsafe extern "C" fn presence_ble_scan_result_builder_add_action(
    builder: *mut PresenceBleScanResultBuilder,
    action: i32,
) {
    (*builder).add_action(action);
}
#[no_mangle]
pub unsafe extern "C" fn presence_ble_scan_result_builder_build(
    builder: *mut PresenceBleScanResultBuilder,
) -> *mut PresenceBleScanResult {
    Box::into_raw(Box::new(Box::from_raw(builder).build()))
}
#[no_mangle]
pub unsafe extern "C" fn presence_request_debug_print(request: *const PresenceDiscoveryRequest) {
    println!("Rust FFI Lib: {:?}", *request);
}

#[no_mangle]
pub extern "C" fn presence_enum_medium_debug_print(presence_medium: PresenceMedium) {
    debug!("Medium type: {:?}", presence_medium)
}

#[cfg(test)]
mod tests {
    use crate::{
        presence_request_builder_add_condition, presence_request_builder_build,
        presence_request_builder_new,
    };
    use presence_core::{PresenceIdentityType, PresenceMeasurementAccuracy};

    #[test]
    fn test_request_builder() {
        unsafe {
            let builder = presence_request_builder_new(1);
            presence_request_builder_add_condition(
                builder,
                10,
                PresenceIdentityType::Private,
                PresenceMeasurementAccuracy::BestAvailable,
            );
            let request = presence_request_builder_build(builder);
            assert_eq!((*request).priority, 1);
            assert_eq!((*request).conditions.len(), 1);
            assert_eq!((*request).conditions[0].action, 10);
        }
    }
}
