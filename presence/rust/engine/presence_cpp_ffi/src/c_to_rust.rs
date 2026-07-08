// Define builders to build data structures passed from C to Rust.
// The builders will be FFIed to C functions, which are used by C codes to build data structures
// passed into Rust.

use log::debug;
use presence_core::ble_scan_provider::PresenceScanResult;
use presence_core::client_provider::{
    PresenceDiscoveryCondition, PresenceDiscoveryRequest, PresenceIdentityType,
    PresenceMeasurementAccuracy, PresenceMedium,
};
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
    medium: PresenceMedium,
) -> *mut PresenceBleScanResultBuilder {
    Box::into_raw(Box::new(PresenceBleScanResultBuilder::new(medium)))
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
) -> *mut PresenceScanResult {
    Box::into_raw(Box::new(Box::from_raw(builder).build()))
}

#[no_mangle]
pub unsafe extern "C" fn presence_request_debug_print(request: *const PresenceDiscoveryRequest) {
    println!("Rust FFI Lib: {:?}", *request);
}

// FFI PresenceMedium from Rust to C.
#[no_mangle]
pub extern "C" fn presence_enum_medium_debug_print(presence_medium: PresenceMedium) {
    debug!("Medium type: {:?}", presence_medium)
}

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

    // Builder itself is consumed to the result.
    pub fn build(self) -> PresenceDiscoveryRequest {
        PresenceDiscoveryRequest::new(self.priority, self.conditions)
    }
}

pub struct PresenceBleScanResultBuilder {
    medium: PresenceMedium,
    actions: Vec<i32>,
}

impl PresenceBleScanResultBuilder {
    pub fn new(medium: PresenceMedium) -> Self {
        Self {
            medium,
            actions: Vec::new(),
        }
    }

    pub fn add_action(&mut self, action: i32) {
        self.actions.push(action);
    }

    // Builder itself is consumed to the result.
    pub fn build(self) -> PresenceScanResult {
        PresenceScanResult {
            medium: self.medium,
            actions: self.actions.to_vec(),
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::c_to_rust::{PresenceBleScanResultBuilder, PresenceDiscoveryRequestBuilder};
    use presence_core::client_provider::{
        PresenceDiscoveryCondition, PresenceIdentityType, PresenceMeasurementAccuracy,
        PresenceMedium,
    };

    #[test]
    fn test_build_presence_discovery_request() {
        let expected_priority = 99;
        let expected_action = 100;
        let expected_identity_type = PresenceIdentityType::Private;
        let expected_accuracy = PresenceMeasurementAccuracy::CoarseAccuracy;
        let mut builder = PresenceDiscoveryRequestBuilder::new(expected_priority);
        builder.add_condition(PresenceDiscoveryCondition {
            action: expected_action,
            identity_type: expected_identity_type,
            measurement_accuracy: expected_accuracy,
        });
        let request = builder.build();
        assert_eq!(request.priority, expected_priority);
        assert_eq!(request.conditions.len(), 1);
        assert_eq!(request.conditions[0].action, expected_action);
        assert_eq!(request.conditions[0].identity_type, expected_identity_type);
        assert_eq!(
            request.conditions[0].measurement_accuracy,
            expected_accuracy
        );
    }

    #[test]
    fn test_build_presence_scan_result() {
        let expected_medium = PresenceMedium::BLE;
        let expected_action_zero = 100;
        let expected_action_one = 101;
        let mut builder = PresenceBleScanResultBuilder::new(expected_medium);
        builder.add_action(expected_action_zero);
        builder.add_action(expected_action_one);
        let result = builder.build();
        assert_eq!(result.medium, expected_medium);
        assert_eq!(result.actions.len(), 2);
        assert_eq!(result.actions[0], expected_action_zero);
        assert_eq!(result.actions[1], expected_action_one);
    }
}
