use std::ptr;
use ffi;

#[derive(Debug)]
// Struct to send a discovery request to the Engine.
// This is a safe clone of `DiscoveryEngineRequest` defined in ffi module.
pub struct DiscoveryEngineRequest {
    pub priority: i32,
    pub conditions: Vec<ffi::DiscoveryCondition>,
}

impl DiscoveryEngineRequest {
    // Returns a `DiscoveryEngineRequest` from the raw buffer of `ffi::DiscoveryEngineRequest`.
    // Caller retains the ownership of `request_ptr`.
    pub unsafe fn from_raw(request_ptr: *const ffi::DiscoveryEngineRequest) -> Self {
        let condition_list = &(*request_ptr).conditions;
        let conditions = copy_from_raw_list(condition_list.items, condition_list.count);
        DiscoveryEngineRequest {
            priority: (*request_ptr).priority,
            conditions,
        }
    }

    // Returns the raw buffer of `ffi::DiscoveryEngineRequest`.
    // Caller owns the raw buffer.
    pub unsafe fn to_raw(self) -> *const ffi::DiscoveryEngineRequest {
        let conditions_copy = self.conditions.clone();
        let conditions = ffi::DiscoveryConditionList {
            items: Box::into_raw(
                conditions_copy.into_boxed_slice()) as *const ffi::DiscoveryCondition,
            count: self.conditions.len(),
        };
        Box::into_raw(Box::new(ffi::DiscoveryEngineRequest {
            priority: self.priority,
            conditions,
        }))
    }
}

unsafe fn copy_from_raw_list<T>(ptr: *const T, count: usize) -> Vec<T> {
    let mut dst = Vec::with_capacity(count);
    ptr::copy_nonoverlapping(ptr, dst.as_mut_ptr(), count);
    dst.set_len(count);
    dst
}

#[cfg(test)]
mod tests {
    use ffi;
    use client_provider::DiscoveryEngineRequest;
    use ffi::DiscoveryCondition;

    #[test]
    fn test_request_from_raw() {
        let conditions: Vec<ffi::DiscoveryCondition> = Vec::from([
            ffi::DiscoveryCondition { action: 1, identity_type: 0, measurement_accuracy: 0 },
            ffi::DiscoveryCondition { action: 2, identity_type: 0, measurement_accuracy: 0 }
        ]);
        let raw_conditions = ffi::DiscoveryConditionList {
            items: Box::into_raw(conditions.into_boxed_slice()) as *const ffi::DiscoveryCondition,
            count: 2,
        };
        let raw_request = Box::into_raw(Box::new(ffi::DiscoveryEngineRequest {
            priority: 3,
            conditions: raw_conditions,
        }));
        unsafe {
            let request = DiscoveryEngineRequest::from_raw(raw_request);
            assert_eq!(request.priority, 3);
            assert_eq!(request.conditions.len(), 2);
            assert_eq!(request.conditions[0].action, 1);
            assert_eq!(request.conditions[1].action, 2);
        }
    }

    #[test]
    fn test_request_to_raw() {
        let conditions: Vec<DiscoveryCondition> = Vec::from([
            DiscoveryCondition { action: 1, identity_type: 0, measurement_accuracy: 0 },
            DiscoveryCondition { action: 2, identity_type: 0, measurement_accuracy: 0 }
        ]);
        let request = DiscoveryEngineRequest {
            priority: 3,
            conditions,
        };
        unsafe {
            let request_ptr = request.to_raw();
            assert_eq!((*request_ptr).priority, 3);
            assert_eq!((*request_ptr).conditions.count, 2);
            assert_eq!((*(*request_ptr).conditions.items).action, 1);
            assert_eq!((*(*request_ptr).conditions.items.add(1)).action, 2);
        }
    }
}