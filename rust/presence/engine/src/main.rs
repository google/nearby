use std::thread::sleep;
use std::time::Duration;
use crate::client_provider::Credential;

mod fused_engine;
mod ble_scan_provider;
mod ble_scan_emulator;
mod client_provider;
mod provider;
mod client_emulator;
mod engine;
mod timer_provider;
mod scan_controller;
mod timer_controller;

fn main() {
    let engine = std::thread::spawn(|| { engine::run(); });
    std::thread::spawn(|| {
        // Waits for Engine to run.
        sleep(Duration::new(1, 0));
        let mut credentials: Vec< crate::client_provider::Credential> = Vec::new();
        credentials.push(Credential{});
        client_emulator::start_discovery(client_provider::DiscoveryEngineRequest{
            credentials, action: 1});
    });
    engine.join().unwrap();
}