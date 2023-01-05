// Copyright 2022 Google LLC
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

#include "internal/platform/implementation/windows/bluetooth_pairing.h"

#include "internal/platform/logging.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {

namespace {
using ::winrt::Windows::Devices::Enumeration::DeviceInformationCustomPairing;
using ::winrt::Windows::Devices::Enumeration::DevicePairingKinds;
using ::winrt::Windows::Devices::Enumeration::DevicePairingProtectionLevel;
using ::winrt::Windows::Devices::Enumeration::DevicePairingRequestedEventArgs;
using ::winrt::Windows::Devices::Enumeration::DevicePairingResult;
using ::winrt::Windows::Devices::Enumeration::DevicePairingResultStatus;
using ::winrt::Windows::Foundation::IAsyncOperation;
}  // namespace

BluetoothPairing::BluetoothPairing(
    DeviceInformationCustomPairing& custom_pairing)
    : custom_pairing_(custom_pairing) {}

BluetoothPairing::~BluetoothPairing() = default;

void BluetoothPairing::StartPairing() {
  NEARBY_LOGS(VERBOSE) << "Bluetooth_pairing start pairing";
  pairing_requested_token_ = custom_pairing_.PairingRequested(
      {this, &BluetoothPairing::OnPairingRequested});
  IAsyncOperation<DevicePairingResult> pairing_operation =
      custom_pairing_.PairAsync(DevicePairingKinds::ConfirmOnly |
                                    DevicePairingKinds::ProvidePin |
                                    DevicePairingKinds::ConfirmPinMatch,
                                DevicePairingProtectionLevel::None);
  DevicePairingResult pairing_result = pairing_operation.get();
  if (pairing_result != nullptr) {
    OnPair(pairing_result);
  }
}

void BluetoothPairing::OnPairingRequested(
    DeviceInformationCustomPairing custom_pairing,
    DevicePairingRequestedEventArgs pairing_requested) {
  NEARBY_LOGS(INFO) << "BluetoothPairing::OnPairingRequested()";
  DevicePairingKinds pairing_kind = pairing_requested.PairingKind();
  switch (pairing_kind) {
    case DevicePairingKinds::ProvidePin:
      NEARBY_LOGS(INFO) << "DevicePairingKind: RequestPinCode.";
      pairing_requested.Accept();
      return;
    case DevicePairingKinds::ConfirmOnly:
      NEARBY_LOGS(INFO) << "DevicePairingKind: ConfirmOnly.";
      pairing_requested.Accept();
      break;
    case DevicePairingKinds::ConfirmPinMatch:
      NEARBY_LOGS(INFO) << "DevicePairingKind: Confirm Pin Match： "
                        << pairing_requested.Pin().c_str();
      pairing_requested.Accept();
      break;
    default:
      NEARBY_LOGS(INFO) << "Unsupported DevicePairingKind = "
                        << static_cast<int>(pairing_kind);
      break;
  }
}

void BluetoothPairing::OnPair(DevicePairingResult& pairing_result) {
  DevicePairingResultStatus status = pairing_result.Status();

  switch (status) {
    case DevicePairingResultStatus::AlreadyPaired:
    case DevicePairingResultStatus::Paired:
      NEARBY_LOGS(ERROR) << "Pairing Result Status: Paired.";
      return;
    case DevicePairingResultStatus::PairingCanceled:
      NEARBY_LOGS(ERROR) << "Pairing Result Status: Pairing Canceled.";
      return;
    case DevicePairingResultStatus::AuthenticationFailure:
      NEARBY_LOGS(ERROR) << "Pairing Result Status: Authentication Failure.";
      return;
    case DevicePairingResultStatus::ConnectionRejected:
    case DevicePairingResultStatus::RejectedByHandler:
      NEARBY_LOGS(ERROR) << "Pairing Result Status: Authentication Rejected.";
      return;
    case DevicePairingResultStatus::AuthenticationTimeout:
      NEARBY_LOGS(ERROR) << "Pairing Result Status: Authentication Timeout.";
      return;
    case DevicePairingResultStatus::Failed:
      NEARBY_LOGS(ERROR) << "Pairing Result Status: Failed.";
      return;
    case DevicePairingResultStatus::OperationAlreadyInProgress:
      NEARBY_LOGS(ERROR) << "Pairing Result Status: Operatio In Progress.";
      return;
    default:
      NEARBY_LOGS(ERROR) << "Pairing Result Status: Failed.";
      return;
  }
}

}  // namespace windows
}  // namespace nearby
