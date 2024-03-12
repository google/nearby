use std::future::Future;
use std::pin::Pin;
use std::time::{Duration, SystemTime};
use futures::future::BoxFuture;
use futures::stream::{FuturesOrdered, FuturesUnordered};
use futures::StreamExt;
use log::{error, info};
use tokio::select;
use tokio::sync::mpsc;
use tokio::time::sleep;
use crate::fused_engine::ProviderEvent;

pub struct AlarmEvent {
    // Time to start to count down.
    pub scheduled_time: SystemTime,
    pub delay_duration: Duration,
}

pub struct TimerProvider {
    // Used by Controller to send events to Provider.
    // controller_tx will be cloned to Controller.
    controller_rx: mpsc::Receiver<AlarmEvent>,

    provider_tx: mpsc::Sender<ProviderEvent>,

    timers: FuturesUnordered<Pin<Box<dyn Future<Output=AlarmEvent> + Send>>>,
}

impl TimerProvider {
    pub fn new(provider_tx: mpsc::Sender<ProviderEvent>, timer_control_tx: mpsc::Receiver<AlarmEvent>) -> Self {
        Self {
            controller_rx: timer_control_tx,
            provider_tx,
            timers: FuturesUnordered::new(),
        }
    }

    pub async fn run(&mut self) {
        loop {
            select! {
              Some(timer_start_event) = self.controller_rx.recv() => {
                    info!("Timer Provider time starts {:?}", timer_start_event.scheduled_time);
                    self.add_timer(timer_start_event);
               }
               Some(timeout_event) = self.timers.next() => {
                    info!("Timer Provider timeout {:?}", timeout_event.scheduled_time);
                   if let Err(e) = self.provider_tx.send(
                        ProviderEvent::Timer(timeout_event)).await {
                       error!("Timer failed to send a timeout event {}", e);
                   } else {
                       info!("Timer succeeds to send timeout event.");
                   }
               }
           }
        }
    }

    fn add_timer(&mut self, event: AlarmEvent) {
        self.timers.push(Box::pin(async {
            sleep(event.delay_duration).await;
            event
        }))
    }
}