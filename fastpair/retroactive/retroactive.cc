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

#include "fastpair/retroactive/retroactive.h"

#include <memory>
#include <optional>

#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "third_party/magic_enum/magic_enum.hpp"
#include "fastpair/common/constant.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/fast_pair_controller.h"
#include "fastpair/message_stream/message_stream.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/direct_executor.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr absl::string_view kSeekerAddress = "11:12:13:14:15:16";
}
Retroactive::Retroactive(FastPairController* controller) noexcept
    : controller_(controller) {
  message_stream_connection_observer_ = {
      .on_connected =
          [this](const FastPairDevice& device) {
            NEARBY_LOGS(INFO) << "Message stream connected";
            SetPairingStep(PairingStep::kWaitForModelIdAndBleAddress);
          },
      .on_disconnected =
          [this](const FastPairDevice& device, absl::Status status) {
            NEARBY_LOGS(INFO) << "Message stream disconnected";
            if (pairing_step_ == PairingStep::kNewDevicePairedOrConnected) {
              // New device is disconnected. Not an error.
              SetPairingStep(PairingStep::kOpenMessageStream);
              return;
            }
            pairing_result_.Set(status);
            SetPairingStep(PairingStep::kFailed);
          }};
  address_rotation_observer_ = {
      .on_ble_address_rotated = [this](const FastPairDevice& device,
                                       absl::string_view old_address) {
        NEARBY_LOGS(INFO) << "Received ble address";
        if (pairing_step_ != PairingStep::kWaitForModelIdAndBleAddress) {
          return;
        }
        if (!device.GetBleAddress().empty() && !device.GetModelId().empty()) {
          SetPairingStep(PairingStep::kFetchAntiSpoofingKey);
        }
      }};
  model_id_observer_ = {.on_model_id = [this](const FastPairDevice& device) {
    NEARBY_LOGS(INFO) << "Received model id";
    if (pairing_step_ != PairingStep::kWaitForModelIdAndBleAddress) {
      return;
    }
    if (!device.GetBleAddress().empty() && !device.GetModelId().empty()) {
      SetPairingStep(PairingStep::kFetchAntiSpoofingKey);
    }
  }};
}

Retroactive::~Retroactive() {
  // TODO(jsobczak): Disable `data_encryptor_` listener.
  SetPairingStep(PairingStep::kNone);
}

Future<absl::Status> Retroactive::Pair() {
  SetPairingStep(PairingStep::kNewDevicePairedOrConnected);
  return pairing_result_;
}

void Retroactive::SetPairingStep(PairingStep step) {
  if (pairing_step_ == step) return;
  ExitPairingStep(pairing_step_);
  NEARBY_LOGS(INFO) << "Enter pairing step " << magic_enum::enum_name(step);
  pairing_step_ = step;
  switch (step) {
    case PairingStep::kNone: {
      controller_->RemoveMessageStreamConnectionObserver(
          &message_stream_connection_observer_);
      break;
    }
    case PairingStep::kNewDevicePairedOrConnected: {
      controller_->AddMessageStreamConnectionObserver(
          &message_stream_connection_observer_);
      break;
    }
    case PairingStep::kOpenMessageStream: {
      absl::Status status = controller_->OpenMessageStream();
      if (!status.ok()) {
        SetPairingStep(PairingStep::kFailed);
        return;
      }
      break;
    }
    case PairingStep::kWaitForModelIdAndBleAddress: {
      controller_->AddModelIdObserver(&model_id_observer_);
      controller_->AddAddressRotationObserver(&address_rotation_observer_);
      break;
    }
    case PairingStep::kFetchAntiSpoofingKey: {
      // Fetch ASK and set up data encryptor.
      data_encryptor_ = controller_->GetDataEncryptor();
      data_encryptor_.AddListener(
          [this](ExceptionOr<std::shared_ptr<FastPairDataEncryptor>> result) {
            if (result.ok() && result.result()) {
              SetPairingStep(PairingStep::kSendKeyBasedPairingRequest);
            } else {
              SetPairingStep(PairingStep::kFailed);
            }
          },
          &executor_);
      break;
    }
    case PairingStep::kSendKeyBasedPairingRequest: {
      // Open GATT connection and send Key-based pairing request to provider.
      gatt_client_ = controller_->GetGattClientRef();
      gatt_client_.AddListener(
          [this](ExceptionOr<FastPairController::GattClientRef> gatt) {
            if (!gatt.ok()) {
              NEARBY_LOGS(INFO) << "Creating GATT client failed";
              return;
            }
            NEARBY_LOGS(INFO) << "Sending Key Based Pairing request to "
                              << controller_->GetDevice().GetBleAddress();
            auto decryptor = data_encryptor_.Get().result().get();
            (gatt.result())
                ->WriteRequestAsync(
                    kKeyBasedPairingType, kRetroactiveFlags,
                    controller_->GetDevice().GetBleAddress(),
                    controller_->GetSeekerMacAddress(), *decryptor,
                    [this](absl::string_view response,
                           std::optional<PairFailure> failure) {
                      if (failure.has_value()) {
                        NEARBY_LOGS(WARNING)
                            << "Key-based pairing exchange failed";
                        SetPairingStep(PairingStep::kFailed);
                        return;
                      }
                      NEARBY_LOGS(INFO) << "key based pairing reply "
                                        << absl::BytesToHexString(response);
                      SetPairingStep(PairingStep::kSendAccountKeyToProvider);
                    });
          },
          &executor_);
      break;
    }
    case PairingStep::kSendAccountKeyToProvider: {
      auto decryptor = data_encryptor_.Get().result().get();
      gatt_client_.Get().result()->WriteAccountKey(
          *decryptor, [this](std::optional<AccountKey> account_key,
                             std::optional<PairFailure> failure) {
            if (failure.has_value()) {
              NEARBY_LOGS(WARNING)
                  << "Account key write failed with: " << *failure;
              SetPairingStep(PairingStep::kFailed);
              return;
            }
            DCHECK(account_key.has_value());
            NEARBY_LOGS(INFO)
                << "Sent account key: "
                << absl::BytesToHexString(account_key->GetAsBytes());
            controller_->GetDevice().SetAccountKey(*account_key);
            SetPairingStep(PairingStep::kAskForUserConfirmation);
          });
      break;
    }
    case PairingStep::kAskForUserConfirmation: {
      // TODO(jsobczak): Display UI to ask the user to confirm onboarding the
      // device.
      SetPairingStep(PairingStep::kUploadAccountKeyToCloud);
      break;
    }
    case PairingStep::kUploadAccountKeyToCloud: {
      // TODO(jsobczak): Upload the Account Key to the server.
      SetPairingStep(PairingStep::kSuccess);
      break;
    }
    case PairingStep::kSuccess: {
      pairing_result_.Set(absl::OkStatus());
      break;
    }
    case PairingStep::kFailed: {
      pairing_result_.Set(absl::InternalError("Pairing failed"));
      break;
    }
    default:
      break;
  }
}

void Retroactive::ExitPairingStep(PairingStep step) {
  switch (step) {
    case PairingStep::kWaitForModelIdAndBleAddress: {
      controller_->RemoveModelIdObserver(&model_id_observer_);
      controller_->RemoveAddressRotationObserver(&address_rotation_observer_);
      break;
    }
    default:
      break;
  }
}

}  // namespace fastpair
}  // namespace nearby
