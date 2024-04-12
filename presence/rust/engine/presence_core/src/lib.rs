pub mod ble_scan_provider;
pub mod client_provider;

use crate::ble_scan_provider::{BleScanProvider, BleScanner, PresenceBleScanResult};
use crate::client_provider::{DiscoveryCallback, PresenceClientProvider};
use log::{info, log};
use tokio::runtime::Builder;
use tokio::sync::mpsc;

// The enum is annotated by repr(C) to pass through FFI.
#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C)]
pub enum PresenceIdentityType {
    Private = 0,
    Trusted,
    Public,
}

// The enum is annotated by repr(C) to pass through FFI.
#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C)]
pub enum PresenceMeasurementAccuracy {
    Unknown = 0,
    CoarseAccuracy,
    BestAvailable,
}
/// Struct to hold an action, identity type and their associated discovery condition.
#[derive(Clone, Copy, Debug)]
pub struct PresenceDiscoveryCondition {
    pub action: i32,
    pub identity_type: PresenceIdentityType,
    pub measurement_accuracy: PresenceMeasurementAccuracy,
}

#[derive(Debug)]
/// Struct to send a discovery request to the Engine.
pub struct PresenceDiscoveryRequest {
    pub priority: i32,
    pub conditions: Vec<PresenceDiscoveryCondition>,
}

pub struct DiscoveryResult {
    pub priority: i32,
    pub actions: Vec<i32>,
}

impl DiscoveryResult {
    fn new(priority: i32) -> Self {
        Self {
            priority,
            actions: Vec::new(),
        }
    }
    fn add_action(&mut self, action: i32) {
        self.actions.push(action);
    }
}

pub enum ProviderEvent {
    PresenceDiscoveryRequest(PresenceDiscoveryRequest),
    BleScanResult(PresenceBleScanResult),
}

pub struct PresenceEngine {
    // Receive events from Providers.
    provider_rx: mpsc::Receiver<ProviderEvent>,
    client_provider: PresenceClientProvider,
    ble_scan_provider: BleScanProvider,
}

impl PresenceEngine {
    pub fn new(
        provider_tx: mpsc::Sender<ProviderEvent>,
        provider_rx: mpsc::Receiver<ProviderEvent>,
        discovery_callback: Box<dyn DiscoveryCallback>,
        ble_scanner: Box<dyn BleScanner>,
    ) -> Self {
        let client_provider = PresenceClientProvider::new(provider_tx.clone(), discovery_callback);
        let ble_scan_provider = BleScanProvider::new(provider_tx, ble_scanner);
        Self {
            provider_rx,
            client_provider,
            ble_scan_provider,
        }
    }

    pub fn get_client_provider(&self) -> &PresenceClientProvider {
        &self.client_provider
    }

    pub fn get_ble_scan_provider(&self) -> &BleScanProvider {
        &self.ble_scan_provider
    }
    pub fn run(&mut self) {
        info!("Presence Engine run.");
        Builder::new_current_thread()
            .build()
            .unwrap()
            .block_on(async move {
                self.poll_providers().await;
            });
    }

    async fn poll_providers(&mut self) {
        // loop to receive events from Providers and process the event according to its type.
        loop {
            info!("loop to receive provider events.");
            if let Some(event) = self.provider_rx.recv().await {
                match event {
                    ProviderEvent::PresenceDiscoveryRequest(request) => {
                        info!("received discovery request: {:?}.", request);
                        self.ble_scan_provider.start_ble_scan(request);
                    }
                    ProviderEvent::BleScanResult(result) => {
                        info!("received BLE scan result: {:?}.", result);
                        let mut discovery_result = DiscoveryResult::new(result.priority);
                        for action in result.actions {
                            discovery_result.add_action(action);
                        }
                        self.client_provider.on_device_updated(discovery_result);
                    }
                }
            }
        }
    }
}
