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

mod bluetooth;

use futures::executor;
use futures::stream::StreamExt;

fn main() -> Result<(), anyhow::Error> {
    let run = async {
        let adapter = bluetooth::BleAdapter::default().await?;
        let mut scanner = adapter.scanner()?;

        while let Some(device) = scanner.next().await {
            println!("found {}", device.name()?)
        }

        unreachable!("Done scanning");
    };

    executor::block_on(run)
}
