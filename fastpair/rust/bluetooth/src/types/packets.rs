// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Specification: https://developers.google.com/nearby/fast-pair/specifications/extensions/messagestream
// This file should be in sync with fastpair/message_stream/message.h.


#[derive(Debug)]
pub enum BluetoothCode {
    EnableSilenceMode = 0x01,
    DisableSilenceMode = 0x02,
}

#[derive(Debug)]
pub enum CompanionAppEventCode {
    LogBufferFull = 0x01,
}

#[derive(Debug)]
pub enum DeviceInformationEventCode {
    ModelId = 0x01,
    BleAddressUpdated = 0x02,
    BatteryUpdated = 0x03,
    RemainingBattery = 0x04,
    ActiveComponentsRequest = 0x05,
    ActiveComponentsResponse = 0x06,
    Capabilities = 0x07,
    PlatformType = 0x08,
    SessionNonce = 0x0A,
}

#[derive(Debug)]
pub enum SassCode {
    Acknowledgement = 0xFF,
    SassGetCapability = 0x10,
    SassNotifyCapability = 0x11,
    SassSetMultipointState = 0x12,
    SassSetSwitchingPreference = 0x20,
    SassGetSwitchingPreference = 0x21,
    SassNotifySwitchingPreference = 0x22,
    SassSwitchActiveSourceCode = 0x30,
    SassSwitchBackAudioSource = 0x31,
    SassNotifyMultipointSwitchEvent = 0x32,
    SassGetConnectionStatus = 0x33,
    SassNotifyConnectionStatus = 0x34,
    SassNotifySassInitiatedConnection = 0x40,
    SassInUseAccountKey = 0x41,
    SassSendCustomData = 0x42,
    SassSetDropConnectionTarget = 0x43,
}

#[derive(Debug)]
pub enum DeviceActionEventCode {
    Ring = 1,
}

#[derive(Debug)]
pub enum AcknowledgementCode {
    Ack = 1,
    Nack = 2,
}

#[derive(Debug)]
pub enum MessageGroup {
    Bluetooth(BluetoothCode),
    CompanionAppEvent(CompanionAppEventCode),
    DeviceInformationEvent(DeviceInformationEventCode),
    DeviceActionEvent(DeviceActionEventCode),
    Sass(SassCode),
    Acknowledgement(AcknowledgementCode),
}


/// A packet that is sent over the RFCOMM Message Stream.
pub struct MessageStreamPacket {
    pub group: MessageGroup,
    pub additional_data: Vec<u8>,
}