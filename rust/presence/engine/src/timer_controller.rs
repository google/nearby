use std::time::{Duration, SystemTime};
use log::info;
use tokio::sync::mpsc;
use crate::timer_provider::AlarmEvent;

#[derive(Clone)]
pub struct TimerController {
    // channel to send commands to Timer Provider.
    timer_tx: mpsc::Sender<AlarmEvent>,
}

impl TimerController {
    pub fn new(timer_tx: mpsc::Sender<AlarmEvent>) -> Self {
        TimerController{
            timer_tx,
        }
    }
    // Schedule an alarm to be triggered after duration of delay.
    pub async fn add_alarm (&self, alarm_event: AlarmEvent) {
        info!("Schedule an alarm in {:?}.", alarm_event.delay_duration);
        self.timer_tx.send(alarm_event).await;
    }
}