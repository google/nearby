mod ble_scan_emulator;
mod ble_scan_provider;
pub mod client_emulator;
pub mod client_provider;
mod fused_engine;
mod provider;
mod scan_controller;
mod timer_controller;
mod timer_provider;

use tokio::runtime::Builder;
use tokio::sync::mpsc;
use tokio::task::JoinSet;
use crate::ble_scan_provider::BleScanProvider;
use crate::client_provider::{ClientDataMsg, ClientProvider};
use crate::fused_engine::FusedEngine;
use crate::scan_controller::ScanControlMsg;
use crate::timer_provider::{AlarmEvent, TimerProvider};

pub fn run() {
    env_logger::init();

    // Channel for Providers to send events to Engine.
    let (provider_event_tx, provider_event_rx)
        = mpsc::channel::<crate::fused_engine::ProviderEvent>(100);
    // Channel for Engine to send data to Client.
    let(client_tx, client_rx) = mpsc::channel::<ClientDataMsg>(100);
    // Channel for Scan Controller to send commands to BLE Scan Provider.
    let (ble_scan_control_tx, ble_scan_control_rx)= mpsc::channel::<ScanControlMsg>(100);
    // Channel for Timer Controller to send commands to Timer Provider.
    let (timer_control_tx, timer_control_rx) = mpsc::channel::<AlarmEvent>(100);

    let mut client_provider = ClientProvider::new(provider_event_tx.clone(), client_rx);
    let mut ble_scan_provider = BleScanProvider::new(provider_event_tx.clone(), ble_scan_control_rx);
    let mut timer_provider = TimerProvider::new(provider_event_tx.clone(), timer_control_rx);
    let mut fused_engine = FusedEngine::new(
        provider_event_rx,
        client_tx,
        ble_scan_control_tx,
        timer_control_tx);

    Builder::new_current_thread()
        .enable_time()
        .build()
        .unwrap().block_on(async move {
        // tokio::join!(client_provider.run(), ble_scan_provider.run(),
        //     timer_provider.run(), fused_engine.run());
        let mut task_set = JoinSet::new();
        task_set.spawn(async move { client_provider.run().await });
        task_set.spawn(async move { ble_scan_provider.run().await });
        task_set.spawn(async move { timer_provider.run().await; });
        task_set.spawn(async move { fused_engine.run().await; });
        task_set.join_next().await;
    });
}