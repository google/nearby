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

use std::error::Error;

use futures::executor;

mod bluetooth;

use bluetooth::{Adapter, Address, ClassicAddress, Device};

fn main() -> Result<(), Box<dyn Error>> {
    let run = async {
        let mut adapter = bluetooth::default_adapter().await?;
        adapter.start_scan()?;

        while let Ok(ble_device) = adapter.next_device().await {
            let name = ble_device.name()?;

            if name.contains("LE_WF-1000XM3") {
                println!("FOUND {} ", name);

                let addr: Address = ble_device.address();

                // Dynamic dispatch is necessary here because `BleDevice` and
                // `ClassicDevice` share the `Device` trait (and thus must have
                // the same return type for `address()` method). This can be
                // changed later if `Device` trait should exclusively define
                // shared cross-platform behavior.
                let classic_addr = match addr {
                    Address::Ble(ble) => ClassicAddress::try_from(ble),
                    Address::Classic(_) => unreachable!(
                        "Address should come from BLE Device, therefore \
                        shouldn't be Classic."
                    ),
                }?;

                let classic_device =
                    bluetooth::new_classic_device(classic_addr).await?;

                match classic_device.pair().await {
                    Ok(_) => {
                        println!("Pairing success!");
                    }
                    Err(err) => println!("Error {}", err),
                }
                break;
            }
        }
        println!("Done scanning");
        Ok(())
    };

    executor::block_on(run)
}
