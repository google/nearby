use std::iter::Scan;
use std::time::{Duration, SystemTime};
use log::{error, info};
use tokio::sync::mpsc;
use crate::client_provider::{ClientDataMsg, Credential, DiscoveryEngineRequest, DiscoveryEngineResult};
use crate::timer_controller::TimerController;
use crate::timer_provider::AlarmEvent;

// Control message sent from Scan Controller to Providers.
#[derive(Debug)]
pub enum ScanControlMsg {
    ScanOptions(ScanOptions),
}

#[derive(Debug)]
pub struct ScanOptions {}

pub struct ScanController {
    // Channel to send scan control messages to BLE scan Provider.
    // Established by the BLE scan Provider when it is added.
    ble_scan_tx: mpsc::Sender<ScanControlMsg>,
}

pub struct ScanData {
    // Placeholder. Defined by structure data instead of raw bytes now.
    pub action: i32,
}

pub enum ScanDataMsg {
    ScanData(ScanData),
}

impl ScanController {
    pub fn new(ble_scan_tx: mpsc::Sender<ScanControlMsg>) -> Self {
        Self {
            ble_scan_tx,
        }
    }

    pub async fn update_scan(&mut self, options: ScanOptions) {
        if let Err(e) = self.ble_scan_tx.send(ScanControlMsg::ScanOptions({options})).await {
            error!("Fused Engine failed to send a command to BLE scan Provider {}.", e);
            return;
        } else {
            info!("Fused Engine sent a command to BLE scan Provider.");
        }
    }

    pub async fn process_scan_event(&self, credentials: &Vec<Credential>,
                                    scan_data_msg: ScanDataMsg) ->Vec<DiscoveryEngineResult> {
        info!("Fused Engine Receives an event from BLE scan Provider.");
        let results = match scan_data_msg {
            ScanDataMsg::ScanData(scan_data) => Self::decode_presence(credentials, &scan_data),
            _ => Vec::new(),
        };
        results
    }

    fn decode_presence(credentials: &Vec<Credential>,
                       scan_data: &ScanData) -> Vec<DiscoveryEngineResult> {
        let mut results: Vec<DiscoveryEngineResult> = Vec::new();
        let time_stamp = SystemTime::now();
        results.push(DiscoveryEngineResult{identity: 1, action: scan_data.action, time_stamp});
        results
    }
}