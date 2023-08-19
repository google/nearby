// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use std::sync::Arc;

use async_trait::async_trait;
use futures::{channel::mpsc::Receiver, StreamExt};
use tracing::{error, info, warn};
use windows::{
    Devices::Bluetooth::{
        Advertisement::{
            // Struct that receives Bluetooth Low Energy (LE) advertisements.
            // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcher?view=winrt-22621
            BluetoothLEAdvertisementReceivedEventArgs,

            // Enum describing the type of advertisement (connectable, directed, etc).
            // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothaddresstype?view=winrt-22621
            BluetoothLEAdvertisementType,

            // Provides data for a Received event on a `BluetoothLEAdvertisementWatcher`.
            // Instance is created when the Received event occurs in the watcher struct.
            // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementreceivedeventargs?view=winrt-22621
            BluetoothLEAdvertisementWatcher,

            // Provides data for a Stopped event on a `BluetoothLEAdvertisementWatcher`.
            // Instance is created when the Stopped event occurs on a watcher struct.
            // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcherstoppedeventargs?view=winrt-22621
            BluetoothLEAdvertisementWatcherStoppedEventArgs,

            // Defines constants that specify a Bluetooth LE scanning mode.
            // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothlescanningmode?view=winrt-22621
            BluetoothLEScanningMode,
        },
        // Struct for obtaining global constant information about a computer's
        // Bluetooth adapter.
        // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetoothadapter?view=winrt-22621
        BluetoothAdapter,
    },
    // Wraps a closure for handling events associated with a struct
    // (e.g. Received and Stopped events in BluetoothLEAdvertisementWatcher).
    // https://learn.microsoft.com/en-us/uwp/api/windows.foundation.typedeventhandler-2?view=winrt-22621
    Foundation::TypedEventHandler,
};

use crate::{
    api,
    common::{BleAdvertisement, BleDataTypeId, BluetoothError},
};

/// Struct holding the necessary fields for listening to and handling incoming
/// BLE advertisements.
struct AdvListener {
    /// Holds callback for sending received advertisement events to `receiver`.
    watcher: BluetoothLEAdvertisementWatcher,
    /// Can be polled to consume incoming advertisement events.
    receiver: Receiver<BluetoothLEAdvertisementReceivedEventArgs>,
}

/// Concrete type implementing `api::BleAdapter`, used for Windows BLE.
pub struct BleAdapter {
    inner: BluetoothAdapter,
    listener: Option<AdvListener>,
}

#[async_trait]
impl api::BleAdapter for BleAdapter {
    async fn default() -> Result<Self, BluetoothError> {
        let inner = BluetoothAdapter::GetDefaultAsync()?.await?;

        if !inner.IsLowEnergySupported()? {
            return Err(BluetoothError::NotSupported(String::from(
                "LE transport type",
            )));
        }
        if !inner.IsCentralRoleSupported()? {
            return Err(BluetoothError::NotSupported(String::from(
                "central role",
            )));
        }

        Ok(BleAdapter {
            inner,
            listener: None,
        })
    }

    fn start_scan(&mut self) -> Result<(), BluetoothError> {
        let watcher = BluetoothLEAdvertisementWatcher::new()?;
        match watcher.SetScanningMode(BluetoothLEScanningMode::Active) {
            Ok(_) => (),
            Err(err) => {
                warn!("Failed to turn on active scanning. Error: {}", err)
            }
        };

        if self.inner.IsExtendedAdvertisingSupported()? {
            watcher.SetAllowExtendedAdvertisements(true)?;
        }

        // `futures::channel::mpsc` is like `std::sync::mpsc` but `impl Stream`.
        let (sender, receiver) = futures::channel::mpsc::channel(16);
        let sender = Arc::new(std::sync::Mutex::new(sender));

        // `received_handler` closure holds non-owning channel reference, to
        // ensure `stopped_handler` can close the channel when
        // `received_handler` is done.
        let weak_sender = Arc::downgrade(&sender);
        let received_handler = TypedEventHandler::new(
            // Move `weak_sender` into closure.
            move |watcher: &Option<BluetoothLEAdvertisementWatcher>,
                  event_args: &Option<
                BluetoothLEAdvertisementReceivedEventArgs,
            >| {
                if watcher.is_some() {
                    if let Some(event_args) = event_args {
                        if let Some(sender) = weak_sender.upgrade() {
                            match sender
                                .lock()
                                .unwrap()
                                .try_send(event_args.clone())
                            {
                                Ok(_) => (),
                                Err(err) => {
                                    error!("Error while handling Received event: {:?}", err)
                                }
                            }
                        }
                    }
                }

                Ok(())
            },
        );

        // `stopped_handler` closure owns channel reference, can close channel.
        let mut sender = Some(sender);
        let stopped_handler = TypedEventHandler::new(
            // Move `sender` into closure.
            move |_watcher,
                  _event_args: &Option<
                BluetoothLEAdvertisementWatcherStoppedEventArgs,
            >| {
                // Drop `sender`, closing the channel.
                let _sender = sender.take();
                info!("Watcher stopped receiving BLE advertisements.");
                Ok(())
            },
        );

        watcher.Received(&received_handler)?;
        watcher.Stopped(&stopped_handler)?;
        watcher.Start()?;

        self.listener = Some(AdvListener { watcher, receiver });

        Ok(())
    }

    fn stop_scan(&mut self) -> Result<(), BluetoothError> {
        if let Some(listener) = self.listener.take() {
            listener.watcher.Stop()?;
            Ok(())
        } else {
            Err(BluetoothError::FailedPrecondition(String::from(
                "device scanning hasn't started, please call `start_scan()`",
            )))
        }
    }

    async fn next_advertisement(
        &mut self,
        datatype_selector: Option<&Vec<BleDataTypeId>>,
    ) -> Result<BleAdvertisement, BluetoothError> {
        if let Some(listener) = &mut self.listener {
            let stream = &mut listener.receiver;
            // We don't want the end-user to receive empty devices, so this is a
            // loop to catch and skip trivial errors from advertisements that
            // can't be turned into devices.
            loop {
                let event_args =
                    stream.next().await.ok_or(BluetoothError::Internal(
                        String::from("Event returned from stream is None."),
                    ))?;

                match event_args.AdvertisementType()? {
                    BluetoothLEAdvertisementType::NonConnectableUndirected => {
                        ()
                    }
                    _ => {
                        let mut advertisement =
                            BleAdvertisement::try_from(&event_args)?;

                        if let Some(datatype_selector) = datatype_selector {
                            advertisement
                                .load_data(&event_args, datatype_selector)?;
                        }

                        break Ok(advertisement);
                    }
                }
            }
        } else {
            Err(BluetoothError::FailedPrecondition(String::from(
                "device scanning hasn't started, please call `start_scan()`",
            )))
        }
    }
}

mod tests {
    // TODO b/288592509 unit tests
}
