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

use windows::Devices::Enumeration::DevicePairingResultStatus;

use crate::common::{BluetoothError, PairingResult};

impl From<windows::core::Error> for BluetoothError {
    fn from(err: windows::core::Error) -> Self {
        BluetoothError::System(err.to_string())
    }
}

// https://learn.microsoft.com/en-us/uwp/api/windows.devices.enumeration.devicepairingresultstatus?view=winrt-22621
impl From<DevicePairingResultStatus> for PairingResult {
    fn from(status: DevicePairingResultStatus) -> Self {
        match status {
            DevicePairingResultStatus::Paired => PairingResult::Success,
            DevicePairingResultStatus::AlreadyPaired => {
                PairingResult::AlreadyPaired
            }
            DevicePairingResultStatus::OperationAlreadyInProgress => {
                PairingResult::AlreadyInProgress
            }
            DevicePairingResultStatus::NotReadyToPair => PairingResult::Failure(
                String::from("the device object is not in a state where it can be paired"),
            ),
            DevicePairingResultStatus::NotPaired => PairingResult::Failure(String::from("the device object is not currently paired.")),
            DevicePairingResultStatus::ConnectionRejected => PairingResult::Failure(String::from("the device object rejected the connection.")),
            DevicePairingResultStatus::TooManyConnections => PairingResult::Failure(String::from("the device object indicated it cannot accept any more incoming connections.")),
            DevicePairingResultStatus::HardwareFailure => PairingResult::Failure(String::from("the device object indicated there was a hardware failure.")),
            DevicePairingResultStatus::AuthenticationTimeout => PairingResult::Failure(String::from("the authentication process timed out before it could complete.")),
            DevicePairingResultStatus::AuthenticationNotAllowed => PairingResult::Failure(String::from("the authentication protocol is not supported, so the device is not paired.")),
            DevicePairingResultStatus::AuthenticationFailure => PairingResult::Failure(String::from("authentication failed, so the device is not paired. Either the device object or the application rejected the authentication.")),
            DevicePairingResultStatus::NoSupportedProfiles => PairingResult::Failure(String::from("there are no network profiles for this device object to use.")),
            DevicePairingResultStatus::ProtectionLevelCouldNotBeMet => PairingResult::Failure(String::from("the minimum level of protection is not supported by the device object or the application.")),
            DevicePairingResultStatus::AccessDenied => PairingResult::Failure(String::from("your application does not have the appropriate permissions level to pair the device object.")),
            DevicePairingResultStatus::InvalidCeremonyData => PairingResult::Failure(String::from("the ceremony data was incorrect.")),
            DevicePairingResultStatus::PairingCanceled => PairingResult::Failure(String::from("the pairing action was canceled before completion.")),
            DevicePairingResultStatus::RequiredHandlerNotRegistered => PairingResult::Failure(String::from("either the event handler wasn't registered or a required DevicePairingKinds was not supported.",)),
            DevicePairingResultStatus::RejectedByHandler => PairingResult::Failure(String::from("the application handler rejected the pairing.")),
            DevicePairingResultStatus::RemoteDeviceHasAssociation => PairingResult::Failure(String::from("the remote device already has an association.")),
            DevicePairingResultStatus::Failed | _ => PairingResult::Failure(String::from("an unknown failure occurred.")),
        }
    }
}
