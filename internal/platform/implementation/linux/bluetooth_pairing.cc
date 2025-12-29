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

#include <utility>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>

#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_pairing.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

void BluetoothPairing::pairing_reply_handler(const sdbus::Error *error) {
  if (error != nullptr && error->isValid()) {
    const auto &name = error->getName();
    api::BluetoothPairingCallback::PairingError err =
        api::BluetoothPairingCallback::PairingError::kAuthFailed;

    LOG(ERROR) << __func__ << ": "
                       << "Got error '" << error->getName()
                       << "' with message '" << error->getMessage()
                       << "' while pairing with device "
                       << device_->GetMacAddress();

    if (name == "org.bluez.Error.AuthenticationCanceled") {
      err = api::BluetoothPairingCallback::PairingError::kAuthCanceled;
    } else if (name == "org.bluez.Error.AuthenticationFailed") {
      err = api::BluetoothPairingCallback::PairingError::kAuthFailed;
    } else if (name == "org.bluez.Error.AuthenticationRejected") {
      err = api::BluetoothPairingCallback::PairingError::kAuthRejected;
    } else if (name == "org.bluez.Error.AuthenticationTimeout") {
      err = api::BluetoothPairingCallback::PairingError::kAuthTimeout;
    }

    if (pairing_cb_.on_pairing_error_cb != nullptr) {
      pairing_cb_.on_pairing_error_cb(err);
    }

    return;
  }
  if (pairing_cb_.on_paired_cb != nullptr) {
    pairing_cb_.on_paired_cb();
  }
}

BluetoothPairing::BluetoothPairing(
    BluetoothAdapter &adapter, std::shared_ptr<BluetoothDevice> remote_device)
    : device_(std::move(remote_device)),
      device_object_path_(bluez::device_object_path(adapter.GetObjectPath(),
                                                    device_->GetAddress().ToString())),
      adapter_(adapter) {}

bool BluetoothPairing::InitiatePairing(
    api::BluetoothPairingCallback pairing_cb) {
  pairing_cb_ = std::move(pairing_cb);
  if (pairing_cb_.on_pairing_initiated_cb != nullptr)
    pairing_cb_.on_pairing_initiated_cb(api::PairingParams{
        api::PairingParams::PairingType::kConsent, std::string()});

  return true;
}

bool BluetoothPairing::FinishPairing(
    std::optional<absl::string_view> pin_code) {
  device_->SetPairReplyCallback([this](const sdbus::Error *error) {
    this->pairing_reply_handler(error);
  });

  auto call = device_->Pair();
  if (!call.has_value()) return false;
  pair_async_call_ = *call;
  return true;
}

bool BluetoothPairing::CancelPairing() {
  if (pair_async_call_.isPending()) {
    pair_async_call_.cancel();
  }

  return device_->CancelPairing();
}

bool BluetoothPairing::Unpair() {
  return adapter_.RemoveDeviceByObjectPath(device_object_path_);
}

bool BluetoothPairing::IsPaired() { return device_->Bonded(); }
}  // namespace linux
}  // namespace nearby
