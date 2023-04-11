
pub enum BluetoothCode {
    EnableSilenceMode = 0x01,
    DisableSilenceMode = 0x02,
}

pub enum CompanionAppEventCode {
    LogBufferFull = 0x01,
}

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

pub enum DeviceActionEventCode {
    Ring = 1,
}

pub enum AcknowledgementCode {
    Ack = 1,
    Nack = 2,
}

pub enum MessageGroup {
    Bluetooth(BluetoothCode),
    CompanionAppEvent(CompanionAppEventCode),
    DeviceInformationEvent(DeviceInformationEventCode),
    DeviceActionEvent(DeviceActionEventCode),
    Sass(SassCode),
    Acknowledgement(AcknowledgementCode),
}