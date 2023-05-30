// Copyright 2020 Google LLC
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

// Specification: https://developers.google.com/nearby/fast-pair/specifications/extensions/messagestream
// This file should be in sync with fastpair/message_stream/message_stream.h.

use crate::types::packets::{MessageGroup, MessageStreamPacket};

struct BatteryInfo {
    is_charging: bool,
}

pub struct MessageStream {
    battery_info: BatteryInfo,
}

impl MessageStream {
    pub fn on_received(&mut self, packet: MessageStreamPacket) {
        match packet.group {
            MessageGroup::Bluetooth(b) => println!("It's Bluetooth {:?}", b),
            MessageGroup::CompanionAppEvent(c) => println!("It's CompanionAppEvent :{:?}", c),
            _ => println!("whatever else"),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::packets::{BluetoothCode, MessageGroup, MessageStreamPacket};

    #[test]
    fn test_on_received() {
        let packet = MessageStreamPacket {
            group: MessageGroup::Bluetooth(BluetoothCode::DisableSilenceMode),
            additional_data: vec![0, 1, 2],
        };
        let mut message = MessageStream {
            battery_info: BatteryInfo { is_charging: false }
        };
        message.on_received(packet);
    }
}
