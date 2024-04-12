use crate::{DiscoveryResult, PresenceDiscoveryRequest, ProviderEvent};
use log::info;
use tokio::sync::mpsc;

pub trait DiscoveryCallback {
    fn on_device_updated(&self, result: DiscoveryResult);
}

pub struct PresenceClientProvider {
    provider_event_tx: mpsc::Sender<ProviderEvent>,
    discovery_callback: Box<dyn DiscoveryCallback>,
}

impl PresenceClientProvider {
    pub fn new(
        provider_event_tx: mpsc::Sender<ProviderEvent>,
        discovery_callback: Box<dyn DiscoveryCallback>,
    ) -> Self {
        Self {
            provider_event_tx,
            discovery_callback,
        }
    }
    pub fn set_request(&self, request: PresenceDiscoveryRequest) {
        if let Err(e) = self
            .provider_event_tx
            .blocking_send(ProviderEvent::PresenceDiscoveryRequest(request))
        {
            info!("Provider callback send error: {}", e);
        } else {
            info!("Provider callback sent an event.");
        }
    }

    pub fn on_device_updated(&self, result: DiscoveryResult) {
        info!("on_device_updated.");
        self.discovery_callback.on_device_updated(result);
    }
}
