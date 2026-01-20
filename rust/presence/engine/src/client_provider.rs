use std::hash::{Hash, Hasher};
use std::i32;
use std::time::SystemTime;
use log::{error, info};
use tokio::runtime::Builder;
use tokio::sync::mpsc;
use crate::fused_engine::ProviderEvent;
use crate::client_emulator::{on_discovered, on_lost};
use crate::provider;

// Place holder for discovery result.
#[derive(Debug)]
pub enum ClientDataMsg {
    DiscoveryEngineResult(DiscoveryEngineResult),
    LostResult(DiscoveryEngineResult),
}

#[derive(Clone, Copy, Debug, Eq)]
pub struct DiscoveryEngineResult {
    pub identity: i32,
    pub action: i32,
    pub time_stamp: SystemTime,
}

impl PartialEq for DiscoveryEngineResult {
    // Ignore the time_stamp. Two discovery results are the same if all their fields are equal
    // except the time_stamps.
    fn eq(&self, other: &Self) -> bool {
        self.identity == other.identity && self.action == other.action
    }
}

impl Hash for DiscoveryEngineResult {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.identity.hash(state);
        self.action.hash(state);
    }
}

pub enum ClientControlMsg {
   DiscoveryEngineRequest(DiscoveryEngineRequest),
}

pub struct DiscoveryEngineRequest {
    pub credentials: Vec<Credential>,
    pub action: i32,
}

pub struct Credential {}
impl DiscoveryEngineRequest {
    pub fn new(credentials: Vec<Credential>, action: i32) -> Self {
       DiscoveryEngineRequest {
           credentials,
           action,
       }
    }
}

// Global variable used by FFI start_discovery() to send commands to Engine.
static mut PROVIDER_EVENT_TX: Option<mpsc::Sender<ProviderEvent>> = None;
// FFI called by Client to update Engine.
pub fn start_discovery(request: DiscoveryEngineRequest) {
    unsafe {
        if (PROVIDER_EVENT_TX.is_none()) {
            panic!("Starts discovery before the Provider channel is initialized.");
        }
        provider::update_engine(PROVIDER_EVENT_TX.as_ref().unwrap().clone(),
                                ProviderEvent::ClientControlMsg(
                                    ClientControlMsg::DiscoveryEngineRequest(request)));
    }
}
pub struct ClientProvider {
    // Used by FusedEngine to send events to the client.
    client_rx: mpsc::Receiver<ClientDataMsg>,
}

impl ClientProvider {
    pub fn new(provider_tx: mpsc::Sender<ProviderEvent>,
               engine_rx: mpsc::Receiver<ClientDataMsg>) -> Self {
        unsafe {
            PROVIDER_EVENT_TX = Some(provider_tx);
        }
        Self {
            client_rx: engine_rx,
        }
    }

    pub async fn run(&mut self) {
        info!("Client Provider starts.");
        loop {
            if let Some(client_data_msg) = self.client_rx.recv().await {
                match client_data_msg {
                    ClientDataMsg::DiscoveryEngineResult(result) => {
                        info!("Client Provider receives {:?}", client_data_msg);
                        on_discovered(result);
                    }
                    ClientDataMsg::LostResult(result) => {
                        on_lost(result);
                    }
                }
            } else {
                info!("Client Provider failed to receive an event from Engine.");
                return;
            }
        }
    }
}