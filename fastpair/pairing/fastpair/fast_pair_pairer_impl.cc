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

#include "fastpair/pairing/fastpair/fast_pair_pairer_impl.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/crypto/fast_pair_message_type.h"
#include "fastpair/handshake/fast_pair_handshake_lookup.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr absl::Duration kInitiatePairingTimeout = absl::Seconds(20);
}  // namespace

// static
FastPairPairerImpl::Factory* FastPairPairerImpl::Factory::g_test_factory_ =
    nullptr;

// static
std::unique_ptr<FastPairPairer> FastPairPairerImpl::Factory::Create(
    FastPairDevice& device, Mediums& medium, SingleThreadExecutor* executor,
    AccountManager* account_manager, OnPairedCallback on_paired_cb,
    OnPairingFailedCallback on_pair_failed_cb,
    OnAccountKeyFailureCallback on_account_failure_cb,
    OnPairingCompletedCallback on_pairing_completed_cb) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(
        device, medium, executor, account_manager, std::move(on_paired_cb),
        std::move(on_pair_failed_cb), std::move(on_account_failure_cb),
        std::move(on_pairing_completed_cb));
  }
  return std::make_unique<FastPairPairerImpl>(
      device, medium, executor, account_manager, std::move(on_paired_cb),
      std::move(on_pair_failed_cb), std::move(on_account_failure_cb),
      std::move(on_pairing_completed_cb));
}

// static
void FastPairPairerImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairPairerImpl::FastPairPairerImpl(
    FastPairDevice& device, Mediums& medium, SingleThreadExecutor* executor,
    AccountManager* account_manager, OnPairedCallback on_paired_cb,
    OnPairingFailedCallback on_pair_failed_cb,
    OnAccountKeyFailureCallback on_account_failure_cb,
    OnPairingCompletedCallback on_pairing_completed_cb)
    : device_(device),
      mediums_(medium),
      executor_(executor),
      account_manager_(account_manager),
      on_paired_cb_(std::move(on_paired_cb)),
      on_pair_failed_cb_(std::move(on_pair_failed_cb)),
      on_account_key_failure_cb_(std::move(on_account_failure_cb)),
      on_pairing_completed_cb_(std::move(on_pairing_completed_cb)) {
  if (device_.GetVersion().value() == DeviceFastPairVersion::kHigherThanV1) {
    // Obtains the established GATT connection for use in the pairing process:
    // confirm the passkey, write the account key, etc.
    fast_pair_handshake_ =
        FastPairHandshakeLookup::GetInstance()->Get(&device_);
    CHECK(fast_pair_handshake_);
    CHECK(fast_pair_handshake_->completed_successfully());
    fast_pair_gatt_service_client_ =
        fast_pair_handshake_->fast_pair_gatt_service_client();
  }
}

void FastPairPairerImpl::StartPairing() {
  executor_->Execute(
      "Start Pairing", [&]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
        NEARBY_LOGS(INFO) << __func__ << device_;
        switch (device_.GetProtocol()) {
          case Protocol::kFastPairInitialPairing:
          case Protocol::kFastPairSubsequentPairing:
            // This timer captures a pairing timeout.
            initiate_pairing_timer_.Start(
                kInitiatePairingTimeout / absl::Milliseconds(1), 0, [&]() {
                  NEARBY_LOGS(WARNING)
                      << __func__
                      << ": Timeout while attempting to initiate "
                         "pairing with device.";
                  NotifyPairingFailed(PairFailure::kPairingTimeout);
                });
            pairing_job_ = std::make_unique<SingleThreadExecutor>();
            pairing_job_->Execute("pair", [this]() { InitiatePairing(); });
            break;
          case Protocol::kFastPairRetroactivePairing:
            // Because the devices are already paired, we will directly write an
            // account key to the Provider after a shared secret is established.
            AttemptSendAccountKey();
            break;
        }
      });
}

// Blocking functions
void FastPairPairerImpl::InitiatePairing() {
  NEARBY_LOGS(INFO) << __func__;
  // TODO(b/278810942) : Check if device lost first
  if (mediums_.GetBluetoothRadio().Enable() &&
      mediums_.GetBluetoothClassic().IsAvailable()) {
    bluetooth_pairing_ = mediums_.GetBluetoothClassic().CreatePairing(
        device_.GetPublicAddress().value());
  }
  if (!bluetooth_pairing_) {
    NotifyPairingFailed(PairFailure::kPairingAndConnect);
    return;
  }
  // Unpair with the remote device before initializing a new pairing request
  if (!bluetooth_pairing_->Unpair()) {
    NotifyPairingFailed(PairFailure::kPairingAndConnect);
    return;
  }
  if (!bluetooth_pairing_->InitiatePairing({
          .on_paired_cb =
              [&]() {
                if (!initiate_pairing_timer_.IsRunning()) {
                  NEARBY_LOGS(INFO)
                      << __func__ << " Initiating pairing has timed out.";
                  return;
                }
                initiate_pairing_timer_.Stop();
                NEARBY_LOGS(INFO) << __func__ << " Paired with " << device_;
                // On Windows, Pair is exactly the same as Connect.
                NotifyPaired();
                executor_->Execute(
                    "Write Accountkey",
                    [&]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                      AttemptSendAccountKey();
                    });
              },
          .on_pairing_error_cb =
              [&](api::BluetoothPairingCallback::PairingError error) {
                NEARBY_LOGS(INFO)
                    << __func__ << "Failed to pair with device  due to error "
                    << static_cast<int>(error);
                NotifyPairingFailed(PairFailure::kPairingAndConnect);
              },
          .on_pairing_initiated_cb =
              [&](api::PairingParams pairingParams) {
                if (!initiate_pairing_timer_.IsRunning()) {
                  NEARBY_LOGS(INFO)
                      << __func__ << " Initiating pairing has timed out.";
                  return;
                }
                NEARBY_LOGS(INFO) << __func__ << "Initiated pairing request.";
                if (device_.GetVersion().value() ==
                    DeviceFastPairVersion::kV1) {
                  NEARBY_LOGS(INFO)
                      << __func__
                      << ": For v1 headset, skip passkey confirmation.";
                  bluetooth_pairing_->FinishPairing(std::nullopt);
                } else {
                  NEARBY_LOGS(INFO)
                      << __func__ << ": For headsets higher than v1, "
                      << "confirm passkey before accepting pairing.";
                  ConfirmPasskey(pairingParams);
                }
              },
      })) {
    NotifyPairingFailed(PairFailure::kPairingAndConnect);
  }
}

void FastPairPairerImpl::ConfirmPasskey(api::PairingParams pairingParams) {
  if (!FastPairHandshakeLookup::GetInstance()->Get(&device_)) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": BLE device instance lost during passkey exchange";
    bluetooth_pairing_->CancelPairing();
    NotifyPairingFailed(PairFailure::kDeviceLostMidPairing);
    return;
  }
  expected_passkey_ = pairingParams.passkey;
  NEARBY_LOGS(INFO) << __func__
                    << " Star to confirm passkey: " << expected_passkey_;
  fast_pair_gatt_service_client_->WritePasskeyAsync(
      /*message_type=*/0x02, std::stoi(expected_passkey_),
      *fast_pair_handshake_->fast_pair_data_encryptor(),
      [&](absl::string_view response,
          std::optional<fastpair::PairFailure> failure) {
        OnPasskeyResponse(response, failure);
      });
}

void FastPairPairerImpl::OnPasskeyResponse(absl::string_view response,
                                           std::optional<PairFailure> failure) {
  NEARBY_LOGS(INFO) << __func__;
  if (failure.has_value()) {
    NotifyPairingFailed(failure.value());
    return;
  }
  std::vector<uint8_t> response_bytes(response.begin(), response.end());
  fast_pair_handshake_->fast_pair_data_encryptor()->ParseDecryptPasskey(
      response_bytes, [&](const std::optional<DecryptedPasskey> passkey) {
        OnParseDecryptedPasskey(passkey);
      });
}

void FastPairPairerImpl::OnParseDecryptedPasskey(
    std::optional<DecryptedPasskey> passkey) {
  if (!passkey.has_value()) {
    NotifyPairingFailed(PairFailure::kPasskeyDecryptFailure);
    return;
  }
  if (passkey->message_type != FastPairMessageType::kProvidersPasskey) {
    NEARBY_LOGS(WARNING)
        << "Incorrect message type from decrypted passkey. Expected: "
        << FastPairMessageType::kProvidersPasskey
        << ". Actual: " << passkey->message_type;
    NotifyPairingFailed(PairFailure::kIncorrectPasskeyResponseType);
    return;
  }

  if (passkey->passkey != std::stoi(expected_passkey_)) {
    NEARBY_LOGS(ERROR) << "Passkeys do not match. "
                       << "Expected: " << expected_passkey_
                       << ". Actual: " << std::to_string(passkey->passkey);
    NotifyPairingFailed(PairFailure::kPasskeyMismatch);
    return;
  }
  // TODO(b/278810942) : Check if device lost
  NEARBY_LOGS(INFO) << __func__ << ": Passkeys match, confirming pairing";
  bluetooth_pairing_->FinishPairing(std::nullopt);
}

bool FastPairPairerImpl::CancelPairing() {
  if (!bluetooth_pairing_) {
    NEARBY_LOGS(WARNING) << __func__ << ": No on-going pairing process.";
    return false;
  }
  return bluetooth_pairing_->CancelPairing();
}

bool FastPairPairerImpl::IsPaired() {
  if (!bluetooth_pairing_) {
    return false;
  }
  return bluetooth_pairing_->IsPaired();
}

void FastPairPairerImpl::AttemptSendAccountKey() {
  if (device_.GetVersion().value() == DeviceFastPairVersion::kV1) {
    NotifyPairingCompleted();
    return;
  }

  // We only send the account key if we're doing an initial or retroactive
  // pairing. For subsequent pairing, we have to save the account key
  // locally so that we can refer to it in API calls to the server.
  if (device_.GetProtocol() == Protocol::kFastPairSubsequentPairing) {
    NEARBY_LOGS(VERBOSE) << __func__
                         << ": Saving Account Key locally for subsequent pair";
    // TODO(b/278807993): Saving Account Key locally for subsequent pair
    NotifyPairingCompleted();
    return;
  }

  if (!account_manager_->GetCurrentAccount().has_value()) {
    NEARBY_LOGS(INFO)
        << __func__
        << ": No need to write accountkey because no logged in user.";
    NotifyPairingCompleted();
    return;
  }

  // It's possible that the user has opted to initial pair to a device that
  // already has an account key saved. We check to see if this is the case
  // before writing a new account key.
  if (device_.GetProtocol() == Protocol::kFastPairInitialPairing) {
    FastPairRepository::Get()->IsDeviceSavedToAccount(
        device_.GetPublicAddress().value(), [this](absl::Status status) {
          if (status.ok()) {
            NEARBY_LOGS(VERBOSE)
                << __func__
                << ": Device is already saved, skipping write account key. "
                   "Pairing procedure complete.";
            NotifyPairingCompleted();
            return;
          }
          WriteAccountKey();
        });
  } else {
    // TODO(b/281782018) : Handle BLE address rotation
    WriteAccountKey();
  }
}

void FastPairPairerImpl::WriteAccountKey() {
  fast_pair_gatt_service_client_->WriteAccountKey(
      *fast_pair_handshake_->fast_pair_data_encryptor(),
      [&](const std::optional<AccountKey> account_key,
          const std::optional<PairFailure> failure) {
        OnWriteAccountKey(account_key, failure);
      });
}

void FastPairPairerImpl::OnWriteAccountKey(
    std::optional<AccountKey> account_key, std::optional<PairFailure> failure) {
  if (failure.has_value()) {
    NEARBY_LOGS(WARNING)
        << __func__ << "Failed to write account key to device due to error: "
        << failure.value();
    NotifyAccountKeyFailure(failure.value());
    return;
  }
  if (!account_key.has_value()) {
    NotifyAccountKeyFailure(PairFailure::kAccountKeyCharacteristicWrite);
    return;
  }
  device_.SetAccountKey(account_key.value());

  // Devices in the Retroactive Pair scenario are not written to Footprints
  // on account key write, but when the user hits 'Save' on the retroactive pair
  // notification.
  if (device_.GetProtocol() == Protocol::kFastPairRetroactivePairing) {
    NotifyPairingCompleted();
    return;
  }

  FastPairRepository::Get()->WriteAccountAssociationToFootprints(
      device_, [&](absl::Status status) {
        if (status.ok()) {
          NotifyPairingCompleted();
        } else {
          NotifyAccountKeyFailure(PairFailure::kWriteAccountKeyToFootprints);
        }
      });
}

void FastPairPairerImpl::NotifyPaired() {
  NEARBY_LOGS(INFO) << __func__ << device_;
  executor_->Execute("NotifyPaired",
                     [&, on_paired_cb = std::move(on_paired_cb_)]() mutable {
                       on_paired_cb(device_);
                     });
}

void FastPairPairerImpl::NotifyPairingFailed(PairFailure failure) {
  NEARBY_LOGS(WARNING) << __func__ << ": " << failure;
  // Stop initiate pairing timer as piaring is terminate this time.
  initiate_pairing_timer_.Stop();
  if (!on_pair_failed_cb_) return;
  executor_->Execute("NotifyPairingFailed",
                     [&, on_pair_failed_cb = std::move(on_pair_failed_cb_),
                      failure = std::move(failure)]() mutable {
                       on_pair_failed_cb(device_, failure);
                     });
}

void FastPairPairerImpl::NotifyPairingCompleted() {
  NEARBY_LOGS(INFO)
      << __func__
      << "Account key written to device. Pairing procedure complete.";
  executor_->Execute("Notify Pairing Completed",
                     [&, on_pairing_completed_cb =
                             std::move(on_pairing_completed_cb_)]() mutable {
                       on_pairing_completed_cb(device_);
                     });
}

void FastPairPairerImpl::NotifyAccountKeyFailure(PairFailure failure) {
  NEARBY_LOGS(WARNING) << __func__
                       << "Failed to write account key to device due to error: "
                       << failure;
  executor_->Execute(
      "Notify AccountKey Failure",
      [&, on_account_key_failure_cb = std::move(on_account_key_failure_cb_),
       failure = std::move(failure)]() mutable {
        on_account_key_failure_cb(device_, failure);
      });
}
}  // namespace fastpair
}  // namespace nearby
