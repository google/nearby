use crate::rust_to_c::PresenceDiscoveryResult;
use presence_core::client_provider::{DiscoveryCallback, DiscoveryResult};

pub type PresenceDiscoveryCallback = extern "C" fn(*mut PresenceDiscoveryResult);
pub struct DiscoveryCallbackCpp {
    pub(crate) presence_discovery_callback: crate::PresenceDiscoveryCallback,
}

impl DiscoveryCallback for crate::DiscoveryCallbackCpp {
    fn on_device_update(&self, result: DiscoveryResult) {
        (self.presence_discovery_callback)(PresenceDiscoveryResult::from_discovery_result(result));
    }
}
