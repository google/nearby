use std::collections::HashSet;
use std::time::{Duration, SystemTime};
use log::{error, info};
use tokio::sync::mpsc;
use tokio::time;
use crate::client_provider::{ClientControlMsg, ClientDataMsg, ClientProvider, DiscoveryEngineRequest, DiscoveryEngineResult};
use crate::ble_scan_provider::BleScanProvider;
use crate::{ble_scan_provider, client_provider, timer_provider};
use crate::fused_engine::ProviderEvent::Timer;
use crate::scan_controller::{ScanController, ScanControlMsg, ScanDataMsg, ScanOptions};
use crate::timer_controller::TimerController;
use crate::timer_provider::{AlarmEvent, TimerProvider};


const ON_LOST_TIMEOUT: Duration = Duration::new(2, 0);
// Events sent from Providers to FusedEngine.
pub enum ProviderEvent {
    ClientControlMsg(client_provider::ClientControlMsg),
    ScanDataMsg(ScanDataMsg),
    Timer(timer_provider::AlarmEvent),
}

pub struct FusedEngine {
    // Receive events from Providers.
    provider_rx: mpsc::Receiver<ProviderEvent>,

    client_tx: mpsc::Sender<ClientDataMsg>,
    scan_controller: ScanController,
    timer_controller: TimerController,

    discovery_request: Option<DiscoveryEngineRequest>,
    discovery_results: HashSet<DiscoveryEngineResult>,
}

impl FusedEngine {
    pub fn new(provider_rx:mpsc::Receiver<ProviderEvent>,
               client_tx: mpsc::Sender<ClientDataMsg>,
               ble_scan_tx: mpsc::Sender<ScanControlMsg>,
               timer_tx: mpsc::Sender<AlarmEvent>) -> Self {
        Self {
            provider_rx,
            client_tx,
            scan_controller: ScanController::new(ble_scan_tx),
            timer_controller: TimerController::new(timer_tx),
            discovery_request: None,
            discovery_results: HashSet::new(),
        }
    }

    pub async fn run(&mut self) {
        info!("Fused Engine starts.");

        // loop to receive events from Providers and process the event according to its type.
        loop {
            if let Some(event) = self.provider_rx.recv().await {
                match event {
                    ProviderEvent::ClientControlMsg(client_control_msg) => {
                        self.process_client_provider_event(client_control_msg).await;
                    }
                    ProviderEvent::ScanDataMsg(scan_data_msg) => {
                        self.process_scan_provider_event(scan_data_msg).await;
                    }
                    ProviderEvent::Timer(alarm_event) => {
                        self.process_timer_provider_event(alarm_event).await;
                    }
                }
            }
        }
    }

    async fn process_client_provider_event(&mut self, control_msg: client_provider::ClientControlMsg) {
        info!("Fused Engine Receives a command from Client Provider.");
        match control_msg {
            ClientControlMsg::DiscoveryEngineRequest(request) => {
                self.discovery_request = Some(request);
                self.scan_controller.update_scan(ScanOptions{}).await;
            }
        }
    }

    async fn process_scan_provider_event(&mut self, scan_data_msg: ScanDataMsg) {
        info!("Fused Engine Receives an event from BLE scan Provider.");
        let results = self.scan_controller.process_scan_event(
            self.discovery_request.as_ref().unwrap().credentials.as_ref(),
            scan_data_msg).await;
        let mut timestamp : Option<SystemTime> = None;
        for result in results {
            if result.action == self.discovery_request.as_ref().unwrap().action {
                timestamp = Some(result.time_stamp);
                // This will either add a new result or overwrite the old result with updated
                // timestamp.
                self.discovery_results.replace(result);
                info!("discovery_results size {}", self.discovery_results.len());
                self.client_tx.send(ClientDataMsg::DiscoveryEngineResult(result)).await;
            }
        }
        if (timestamp.is_some()) {
            self.timer_controller.add_alarm(AlarmEvent {
                scheduled_time: timestamp.unwrap(),
                delay_duration: ON_LOST_TIMEOUT,
            }).await;
        }
    }
    async fn process_timer_provider_event(&mut self, timer_event: timer_provider::AlarmEvent) {
        info!("Process timer provider event.");
        let mut lost_results: Vec<DiscoveryEngineResult> = Vec::new();
        for result in &self.discovery_results {
            info!("event timestamp {:?}, result timestamp {:?}", result.time_stamp, timer_event.scheduled_time);
            if result.time_stamp == timer_event.scheduled_time {
                self.client_tx.send(ClientDataMsg::LostResult(result.clone())).await;
                lost_results.push(result.clone());
            }
        }
        for lost_result in &lost_results {
           self.discovery_results.remove(lost_result);
        }
    }
}