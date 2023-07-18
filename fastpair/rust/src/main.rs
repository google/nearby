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

use bluetooth::{Adapter, Device};

fn main() -> Result<(), Box<dyn Error>> {
    let run = async {
        let mut adapter = bluetooth::default_adapter().await?;
        adapter.start_scan_devices()?;

        while let Ok(device) = adapter.next_device().await {
            println!("found {}", device.name()?)
        }

        unreachable!("Done scanning");
    };

    executor::block_on(run)
}
