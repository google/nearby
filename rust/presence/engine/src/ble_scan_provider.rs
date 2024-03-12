use std::sync::mpsc::Receiver;
use log::{error, info};
use tokio::sync::mpsc;
use crate::fused_engine::ProviderEvent;
use crate::ble_scan_emulator;
use crate::ble_scan_emulator::register_provider_tx;
use crate::scan_controller::ScanControlMsg;

pub struct BleScanProvider {
    // Used by FusedEngine to send commands to the Provider.
    engine_rx: mpsc::Receiver<ScanControlMsg>,
}

impl BleScanProvider {
    pub fn new(provider_event_tx: mpsc::Sender<ProviderEvent>, rx: mpsc::Receiver<ScanControlMsg>) -> Self {
        register_provider_tx(provider_event_tx);
        Self {
            engine_rx: rx,
        }
    }

    pub fn set_channels(&mut self, tx: mpsc::Sender<ProviderEvent>) {
        register_provider_tx(tx.clone());
    }

    pub async fn run(&mut self) {
        info!("BLE scan Provider starts.");
        loop {
            if let Some(i) = self.engine_rx.recv().await {
                info!("BLE scan provider receives {:?}", i);
                ble_scan_emulator::start_scan(ble_scan_emulator::BleScanRequest{});
            } else {
                error!("BLE scan Provider failed to receive a command from the fused Engine.");
                return;
            }
        }
    }
}