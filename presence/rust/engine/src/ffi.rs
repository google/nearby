use libc::size_t;
use std::ptr;

/// Struct to hold an action, identity type and their associated discovery condition.
#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct DiscoveryCondition {
    pub action: i32,
    pub identity_type: i32,
    pub measurement_accuracy: i32,
}

/// Struct to hold a list of DiscoveryCondition.
///
/// The `len` is the numer of items in the list.
#[derive(Debug)]
#[repr(C)]
pub struct DiscoveryConditionList {
    pub items: *const DiscoveryCondition,
    pub count: size_t,
}

#[derive(Debug)]
#[repr(C)]
/// Struct to send a discovery request to the Engine.
pub struct DiscoveryEngineRequest {
    pub priority: i32,
    pub conditions: DiscoveryConditionList,
}

#[no_mangle]
pub unsafe extern "C" fn echo_request(
    request_ptr: *const DiscoveryEngineRequest) -> *const DiscoveryEngineRequest {
    println!("Rust receives a DiscoveryEngineRequest.");
    Box::into_raw(Box::new(copy_from_raw_request(request_ptr)))
}

/// Free the memory associated with the [`DiscoveryEngineRequest`](struct.DiscoveryEngineRequest.html) struct.
#[no_mangle]
pub unsafe extern "C" fn free_engine_request(request_ptr: *const DiscoveryEngineRequest) {
    if request_ptr.is_null() { return; }
    let ptr = request_ptr as *mut DiscoveryEngineRequest;
    let request: Box<DiscoveryEngineRequest> = Box::from_raw(ptr);
    let condition_list = request.conditions;
    let _conditions = Vec::from_raw_parts(
        condition_list.items as *mut DiscoveryCondition,
        condition_list.count as usize,
        condition_list.count as usize);
     // Resources will be freed here
}

unsafe fn copy_from_raw_request(
    request_ptr: *const DiscoveryEngineRequest) -> DiscoveryEngineRequest {
    let conditions = &(*request_ptr).conditions;
    let conditions_copy = copy_from_raw_list(conditions.items, conditions.count);
    let condition_list_copy = DiscoveryConditionList {
        items: Box::into_raw(conditions_copy.into_boxed_slice()) as *const DiscoveryCondition,
        count: conditions.count,
    };
    DiscoveryEngineRequest{
        priority: (*request_ptr).priority,
        conditions: condition_list_copy,
    }
}

// Returns a Vector by copying a list of type T from its raw buffer.
unsafe fn copy_from_raw_list<T>(ptr: *const T, count: usize) -> Vec<T> {
    let mut dst = Vec::with_capacity(count);
    ptr::copy_nonoverlapping(ptr, dst.as_mut_ptr(), count);
    dst.set_len(count);
    dst
}

#[cfg(test)]
mod tests {
    use ffi::{copy_from_raw_request, DiscoveryCondition, DiscoveryConditionList,
              DiscoveryEngineRequest};

    #[test]
    fn test_copy_from_raw_request() {
        let conditions: Vec<DiscoveryCondition> = Vec::from([
            DiscoveryCondition{ action: 1, identity_type: 0, measurement_accuracy: 0 },
            DiscoveryCondition{ action: 2, identity_type: 0, measurement_accuracy: 0 }
        ]);
        let raw_condition = DiscoveryConditionList {
            items: Box::into_raw(conditions.into_boxed_slice()) as *const DiscoveryCondition,
            count: 2,
        };
        let raw_request = Box::into_raw(Box::new(DiscoveryEngineRequest {
            priority: 3,
            conditions: raw_condition,
        }));
        unsafe {
            let request = copy_from_raw_request(raw_request);
            assert_eq!(request.priority, 3);
            let conditions: Vec<DiscoveryCondition> = Vec::from_raw_parts(
                request.conditions.items as *mut DiscoveryCondition,
                request.conditions.count,
                request.conditions.count);
            assert_eq!(conditions[0].action, 1);
            assert_eq!(conditions[1].action, 2);

        }
    }
}