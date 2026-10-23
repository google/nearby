use std::sync::mpsc::Receiver;
use log::{error, info};
use tokio::sync::mpsc;
use crate::fused_engine::ProviderEvent;
use crate::{ble_scan_emulator, provider};
use crate::scan_controller::{ScanControlMsg, ScanData, ScanDataMsg};

static mut PROVIDER_TX: Option<mpsc::Sender<ProviderEvent>> = None;
pub fn on_scan_data(scan_data: ScanData) {
    unsafe {
        provider::update_engine(PROVIDER_TX.as_ref().unwrap().clone(),
                                ProviderEvent::ScanDataMsg(ScanDataMsg::ScanData(scan_data)));
    }
}

pub struct BleScanProvider {
    // Used by FusedEngine to send commands to the Provider.
    engine_rx: mpsc::Receiver<ScanControlMsg>,
}

impl BleScanProvider {
    pub fn new(provider_event_tx: mpsc::Sender<ProviderEvent>,
               rx: mpsc::Receiver<ScanControlMsg>) -> Self {
        unsafe {
            PROVIDER_TX = Some(provider_event_tx);
        }
        Self {
            engine_rx: rx,
        }
    }

    pub async fn run(&mut self) {
        info!("BLE scan Provider starts.");
        loop {
            if let Some(scan_control_msg) = self.engine_rx.recv().await {
                info!("BLE scan provider receives {:?}", scan_control_msg);
                ble_scan_emulator::start_scan(ble_scan_emulator::BleScanRequest{});
            } else {
                error!("BLE scan Provider failed to receive a command from the fused Engine.");
                return;
            }
        }
    }
}