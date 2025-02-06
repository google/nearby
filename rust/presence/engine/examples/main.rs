extern crate engine;

use std::thread::sleep;
use std::time::Duration;
use engine::client_provider;

fn main() {
    let engine = std::thread::spawn(|| { engine::run(); });
    std::thread::spawn(|| {
        // Waits for Engine to run.
        sleep(Duration::new(1, 0));
        let mut credentials: Vec< engine::client_provider::Credential> = Vec::new();
        credentials.push( engine::client_provider::Credential{});
        engine::client_emulator::start_discovery(client_provider::DiscoveryEngineRequest{
            credentials, action: 1});
    });
    engine.join().unwrap();
}