use log::info;
use std::thread::sleep;
use std::time::Duration;
use tokio::sync::mpsc;
use crate::{client_provider, provider};
use crate::fused_engine::ProviderEvent;

pub fn start_discovery(request: client_provider::DiscoveryEngineRequest) {
    info!("Client starts discovery.");
    client_provider::start_discovery(request);
}

// FFI called by the client Provider to notify client with a ClientEvent.
pub fn on_discovered(result: client_provider::DiscoveryEngineResult) {
    info!("Client discovers an advertisement with result: {:?}.", result);
}

pub fn on_lost(result: client_provider::DiscoveryEngineResult) {
    info!("Client lost advertisements with result: {:?}.", result);
}
