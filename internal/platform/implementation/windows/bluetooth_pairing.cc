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

#include <windows.h>

#include <exception>
#include <optional>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/windows/generated/winrt/impl/Windows.Devices.Enumeration.0.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {

namespace {
using ::winrt::Windows::Devices::Bluetooth::BluetoothDevice;
using ::winrt::Windows::Devices::Enumeration::DeviceInformationCustomPairing;
using ::winrt::Windows::Devices::Enumeration::DevicePairingKinds;
using ::winrt::Windows::Devices::Enumeration::DevicePairingProtectionLevel;
using ::winrt::Windows::Devices::Enumeration::DevicePairingRequestedEventArgs;
using ::winrt::Windows::Devices::Enumeration::DevicePairingResult;
using ::winrt::Windows::Devices::Enumeration::DevicePairingResultStatus;
using ::winrt::Windows::Devices::Enumeration::DeviceUnpairingResult;
using ::winrt::Windows::Devices::Enumeration::DeviceUnpairingResultStatus;
using ::winrt::Windows::Foundation::IAsyncOperation;
using PairingError = ::nearby::api::BluetoothPairingCallback::PairingError;
using PairingType = ::nearby::api::PairingParams::PairingType;
}  // namespace

BluetoothPairing::BluetoothPairing(
    BluetoothDevice bluetooth_device,
    DeviceInformationCustomPairing custom_pairing)
    : bluetooth_device_(bluetooth_device), custom_pairing_(custom_pairing) {
  NEARBY_LOGS(VERBOSE) << __func__
                       << ": BluetoothPairing is created for device.";
}

BluetoothPairing::~BluetoothPairing() {
  if (pairing_requested_token_) {
    custom_pairing_.PairingRequested(
        std::exchange(pairing_requested_token_, {}));
  }
  CancelPairing();
  NEARBY_LOGS(VERBOSE) << __func__
                       << ": BluetoothPairing is destroyed for device.";
}

bool BluetoothPairing::InitiatePairing(
    api::BluetoothPairingCallback pairing_cb) {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Start to initiate pairing process.";
  try {
    pairing_requested_token_ = custom_pairing_.PairingRequested(
        {this, &BluetoothPairing::OnPairingRequested});
    if (!pairing_requested_token_) {
      NEARBY_LOGS(VERBOSE) << __func__
                           << " Failed to registered pairing callback.";
      return false;
    }
    pairing_callback_ = std::move(pairing_cb);
    DevicePairingResult pairing_result =
        custom_pairing_
            .PairAsync(DevicePairingKinds::ConfirmOnly |
                           DevicePairingKinds::ProvidePin |
                           DevicePairingKinds::ConfirmPinMatch |
                           DevicePairingKinds::DisplayPin,
                       DevicePairingProtectionLevel::None)
            .get();
    OnPair(pairing_result);
    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to initiate pairing. exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to initiate pairing. WinRT exception: "
                       << error.code() << ": "
                       << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

bool BluetoothPairing::FinishPairing(
    std::optional<absl::string_view> pin_code) {
  NEARBY_LOGS(VERBOSE) << __func__ << "Start to finish pairing.";
  try {
    if (!pairing_requested_) {
      NEARBY_LOGS(VERBOSE) << __func__ << "No pairing requested.";
      return false;
    }
    if (!pairing_deferral_) {
      NEARBY_LOGS(VERBOSE) << __func__ << "No ongoing pairing process.";
      return false;
    }
    if (expecting_pin_code_) {
      if (!pin_code.has_value()) {
        NEARBY_LOGS(INFO) << __func__ << " Failed to get pin code";
        return false;
      }
      expecting_pin_code_ = false;
      auto pin_hstring = winrt::to_hstring(std::string(pin_code.value()));
      pairing_requested_.Accept(pin_hstring);
    } else {
      pairing_requested_.Accept();
    }
    pairing_deferral_.Complete();
    NEARBY_LOGS(VERBOSE) << "Successfully finished pairing.";
    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to finish pairing. exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to finish pairing. WinRT exception: "
                       << error.code() << ": "
                       << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

bool BluetoothPairing::CancelPairing() {
  NEARBY_LOGS(VERBOSE) << __func__
                       << " Start to cancel ongoing pairing process.";
  try {
    if (!pairing_deferral_) {
      NEARBY_LOGS(VERBOSE) << __func__ << "No ongoing pairing process.";
      return true;
    }
    // There is no way to explicitly cancel an in-progress pairing on Windows as
    // DevicePairingRequestedEventArgs has no Cancel() method.
    // Our approach is to complete the deferral, without accepting,
    // which results in a RejectedByHandler result status.
    // |was_cancelled_| is set so that OnPair(), which is called when the
    // deferral is completed, will know that cancellation was the actual result.
    was_cancelled_ = true;
    pairing_deferral_.Close();
    NEARBY_LOGS(VERBOSE) << __func__ << "Canceled ongoing pairing process.";
    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to cancel ongoing pairing "
                       << "process. exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to cancel ongoing pairing process. "
                       << "WinRT exception: " << error.code() << ": "
                       << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

bool BluetoothPairing::Unpair() {
  NEARBY_LOGS(VERBOSE) << __func__ << ": Start to unpair with remote device.";
  try {
    if (!IsPaired()) {
      NEARBY_LOGS(VERBOSE) << __func__ << " : Remote device Was not paired.";
      return true;
    }
    DeviceUnpairingResult unpairing_result =
        bluetooth_device_.DeviceInformation().Pairing().UnpairAsync().get();
    if (unpairing_result.Status() == DeviceUnpairingResultStatus::Unpaired) {
      NEARBY_LOGS(VERBOSE) << __func__ << ": Unpaired with remote device.";
      return true;
    }
    NEARBY_LOGS(VERBOSE) << __func__
                         << ": Failed to unpaired with remote device.";
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to unpaired with device. exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to unpaired with device. WinRT exception: "
                       << error.code() << ": "
                       << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

bool BluetoothPairing::IsPaired() {
  try {
    bool is_paired = bluetooth_device_.DeviceInformation().Pairing().IsPaired();
    NEARBY_LOGS(INFO) << __func__ << (is_paired ? " True" : " False");
    return is_paired;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to get IsPaired.  exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to get IsPaired. WinRT exception: "
                       << error.code() << ": "
                       << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
  }
  return false;
}

void BluetoothPairing::OnPairingRequested(
    DeviceInformationCustomPairing custom_pairing,
    DevicePairingRequestedEventArgs pairing_requested) {
  NEARBY_LOGS(VERBOSE) << __func__ << "Requested to pair.";
  try {
    DevicePairingKinds pairing_kind = pairing_requested.PairingKind();
    pairing_requested_ = pairing_requested;
    pairing_deferral_ = pairing_requested.GetDeferral();
    api::PairingParams params;
    switch (pairing_kind) {
      case DevicePairingKinds::ProvidePin:
        NEARBY_LOGS(INFO) << __func__ << "DevicePairingKind: RequestPinCode.";
        expecting_pin_code_ = true;
        params.pairing_type = PairingType::kRequestPin;
        pairing_callback_.on_pairing_initiated_cb(params);
        return;
      case DevicePairingKinds::ConfirmOnly:
        NEARBY_LOGS(INFO) << __func__ << "DevicePairingKind: ConfirmOnly.";
        params.pairing_type = PairingType::kConsent;
        pairing_callback_.on_pairing_initiated_cb(params);
        return;
      case DevicePairingKinds::ConfirmPinMatch:
        NEARBY_LOGS(INFO) << __func__
                          << "DevicePairingKind: Confirm Pin Match.";
        params.pairing_type = PairingType::kConfirmPasskey;
        params.passkey = winrt::to_string(pairing_requested.Pin());
        pairing_callback_.on_pairing_initiated_cb(params);
        return;
      default:
        params.pairing_type = PairingType::kUnknown;
        NEARBY_LOGS(INFO) << __func__ << "Unsupported DevicePairingKind:"
                          << static_cast<int>(pairing_kind);
        break;
    }
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to request to pair with device. exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Failed to request to pair with device. WinRT exception: "
        << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
  }
  pairing_callback_.on_pairing_error_cb(PairingError::kFailed);
}

void BluetoothPairing::OnPair(DevicePairingResult& pairing_result) {
  try {
    DevicePairingResultStatus status = pairing_result.Status();
    NEARBY_LOGS(INFO) << __func__
                      << "Pairing Result Status: " << static_cast<int>(status);
    if (was_cancelled_ &&
        status == DevicePairingResultStatus::RejectedByHandler) {
      // See comment in CancelPairing() for explanation of why was_cancelled_
      // is used.
      status = DevicePairingResultStatus::PairingCanceled;
    }
    switch (status) {
      case DevicePairingResultStatus::AlreadyPaired:
      case DevicePairingResultStatus::Paired:
        NEARBY_LOGS(ERROR) << __func__ << "Pairing Result Status: Paired.";
        pairing_callback_.on_paired_cb();
        return;
      case DevicePairingResultStatus::PairingCanceled:
        NEARBY_LOGS(ERROR) << __func__
                           << "Pairing Result Status: Pairing Canceled.";
        pairing_callback_.on_pairing_error_cb(PairingError::kAuthCanceled);
        return;
      case DevicePairingResultStatus::AuthenticationFailure:
        NEARBY_LOGS(ERROR) << __func__
                           << "Pairing Result Status: Authentication Failure.";
        pairing_callback_.on_pairing_error_cb(PairingError::kAuthFailed);
        return;
      case DevicePairingResultStatus::ConnectionRejected:
      case DevicePairingResultStatus::RejectedByHandler:
        NEARBY_LOGS(ERROR) << __func__
                           << "Pairing Result Status: Authentication Rejected.";
        pairing_callback_.on_pairing_error_cb(PairingError::kAuthRejected);
        return;
      case DevicePairingResultStatus::AuthenticationTimeout:
        NEARBY_LOGS(ERROR) << __func__
                           << "Pairing Result Status: Authentication Timeout.";
        pairing_callback_.on_pairing_error_cb(PairingError::kAuthTimeout);
        return;
      case DevicePairingResultStatus::Failed:
        NEARBY_LOGS(ERROR) << __func__ << "Pairing Result Status: Failed.";
        pairing_callback_.on_pairing_error_cb(PairingError::kFailed);
        return;
      case DevicePairingResultStatus::OperationAlreadyInProgress:
        NEARBY_LOGS(ERROR) << __func__
                           << "Pairing Result Status: Operation In Progress.";
        pairing_callback_.on_pairing_error_cb(PairingError::kRepeatedAttempts);
        return;
      default:
        break;
    }
    NEARBY_LOGS(ERROR) << __func__ << "Pairing Result Status: Failed.";
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Failed to get Pairing Result Status. exception: "
                       << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to get Pairing Result Status."
                       << " WinRT exception: " << error.code() << ": "
                       << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
  }
  pairing_callback_.on_pairing_error_cb(PairingError::kFailed);
}

}  // namespace windows
}  // namespace nearby
