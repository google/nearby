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

use std::{
    collections::HashSet,
    error::Error,
    io::{self, Write},
    sync::Arc,
    thread,
};

use futures::{
    executor::{self, block_on},
    lock::Mutex,
};

extern crate bluetooth;

use bluetooth::{
    api::{BleAdapter, BleDevice, ClassicDevice},
    BleDataTypeId, ClassicAddress, Platform,
};

async fn get_user_input(
    device_vec: Arc<Mutex<Vec<impl BleDevice>>>,
) -> Result<(), Box<dyn Error>> {
    let mut buffer = String::new();
    loop {
        io::stdout().flush()?;
        buffer.clear();
        io::stdin().read_line(&mut buffer)?;

        let val = match buffer.trim().parse::<usize>() {
            Ok(val) => val,
            Err(_) => {
                println!("Please enter a valid digit.");
                continue;
            }
        };

        let index_to_device = device_vec.lock().await;
        match index_to_device.get(val) {
            Some(device) => {
                let addr = device.address();
                let classic_addr = ClassicAddress::try_from(addr)?;

                let classic_device =
                    Platform::new_classic_device(classic_addr).await?;

                match classic_device.pair().await {
                    Ok(_) => {
                        println!("Pairing success!");
                    }
                    Err(err) => println!("Error {}", err),
                }
                break Ok(());
            }
            None => println!("Please enter a valid digit."),
        }
    }
}

fn main() -> Result<(), Box<dyn Error>> {
    let run = async {
        let mut adapter = Platform::default_adapter().await?;
        adapter.start_scan()?;

        let mut addr_set = HashSet::new();
        let device_vec = Arc::new(Mutex::new(Vec::new()));

        {
            // Process user input in a separate thread.
            let device_vec = device_vec.clone();
            thread::spawn(|| block_on(get_user_input(device_vec)).unwrap());
        }

        let mut counter: u32 = 0;
        let datatype_selector = vec![BleDataTypeId::ServiceData16BitUuid];
        // Retrieve incoming device advertisements.
        while let Ok(advertisement) =
            adapter.next_advertisement(Some(&datatype_selector)).await
        {
            for service_data in advertisement.service_data_16bit_uuid()? {
                let uuid = service_data.uuid();

                // This is a Fast Pair device.
                if uuid == 0x2cfe {
                    let addr = advertisement.address();
                    let ble_device = Platform::new_ble_device(addr).await?;
                    let name = ble_device.name()?;

                    if addr_set.insert(addr) {
                        // New FP device discovered.
                        println!("{}: {}", counter, name);
                        device_vec.lock().await.push(ble_device);
                        counter += 1;
                    }
                    break;
                }
            }
        }
        println!("Done scanning");
        Ok(())
    };

    executor::block_on(run)
}
