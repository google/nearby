use log::{error, info};
use tokio::runtime::Builder;
use tokio::sync::mpsc;
use crate::fused_engine::ProviderEvent;


// FFI called by the hosting language to update Engine with events.
pub fn update_engine(provider_tx: mpsc::Sender<ProviderEvent>, event: ProviderEvent) {
    // See https://docs.rs/tokio/1.36.0/tokio/sync/mpsc/index.html#communicating-between-sync-and-async-code
    if let Err(e) = provider_tx.blocking_send(event) {
        error!("Provider callback send error: {}", e);
    } else {
        info!("Provider callback sent an event.");
    }
}