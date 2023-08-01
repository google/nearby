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

#include "fastpair/pairing/pairer_broker_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "fastpair//handshake/fast_pair_handshake_lookup.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/pairing/fastpair/fast_pair_pairer_impl.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/timer_impl.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr int kMaxFailureRetryCount = 3;
constexpr int kMaxNumHandshakeAttempts = 3;
constexpr absl::Duration kCancelPairingRetryDelay = absl::Seconds(1);
constexpr absl::Duration kRetryHandshakeDelay = absl::Seconds(1);
}  // namespace

PairerBrokerImpl::PairerBrokerImpl(Mediums& medium,
                                   SingleThreadExecutor* executor,
                                   AccountManager* account_manager)
    : medium_(medium), executor_(executor), account_manager_(account_manager) {}

void PairerBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PairerBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PairerBrokerImpl::PairDevice(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": Start to pair with device=" << device;
  {
    MutexLock lock(&mutex_);
    model_id_to_current_ble_address_map_.insert_or_assign(
        std::string(device.GetModelId()), std::string(device.GetBleAddress()));
    did_handshake_previously_complete_successfully_map_.insert_or_assign(
        std::string(device.GetModelId()), false);
  }
  PairFastPairDevice(device);
}

void PairerBrokerImpl::PairFastPairDevice(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__;
  {
    MutexLock lock(&mutex_);
    if (fast_pair_pairers_.contains(device.GetModelId())) {
      NEARBY_LOGS(WARNING) << __func__ << ": Already pairing with device"
                           << device;
      return;
    }
  }

  CHECK(device.GetVersion().has_value());
  if (device.GetVersion().value() == DeviceFastPairVersion::kV1) {
    // For v1 headsets, the ble address is the same as the public address.
    // So skip straight to 'StartBondingAttempt'.
    NEARBY_LOGS(INFO) << __func__ << " : Pairing with v1 device.";
    device.SetPublicAddress(device.GetBleAddress());
    StartPairingAttempt(device);
    return;
  }
  NEARBY_LOGS(INFO) << __func__ << " : Pairing with device higher than v1.";
  // For headsets higher than v1,  try to create a handshake with the remote
  // device to request its public address.
  CreateHandshake(device);
}

void PairerBrokerImpl::CreateHandshake(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__;
  {
    MutexLock lock(&mutex_);
    if (device.GetBleAddress() !=
        model_id_to_current_ble_address_map_[device.GetModelId()]) {
      // If the current |device| has a different BLE Address than the address in
      // the map, abort creating the handshake and return early;
      NEARBY_LOGS(INFO)
          << __func__
          << ": The device's BLE did not match the expected value, returning.";
      return;
    }
  }

  auto* fast_pair_handshake =
      FastPairHandshakeLookup::GetInstance()->Get(&device);
  if (fast_pair_handshake) {
    if (fast_pair_handshake->completed_successfully()) {
      NEARBY_LOGS(INFO) << __func__
                        << ": Reusing existing handshake for pair attempt.";
      StartPairingAttempt(device);
      return;
    }
    // If the previous handshake did not complete successfully, erase it
    // before attempting to create a new handshake for the device.
    FastPairHandshakeLookup::GetInstance()->Erase(&device);
  }

  NEARBY_LOGS(INFO) << __func__ << ": Creating new handshake for pair attempt.";
  FastPairHandshakeLookup::GetInstance()->Create(
      device, medium_,
      [&](FastPairDevice& cb_device, std::optional<PairFailure> failure) {
        executor_->Execute("OnHandshakeComplete",
                           [&, failure = std::move(failure)]()
                               ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                                 OnHandshakeComplete(cb_device, failure);
                               });
      },
      executor_);
}

void PairerBrokerImpl::OnHandshakeComplete(FastPairDevice& device,
                                           std::optional<PairFailure> failure) {
  NEARBY_LOGS(INFO) << __func__;
  if (failure.has_value()) {
    OnHandshakeFailure(device, failure.value());
    return;
  }
  if (!device.GetPublicAddress().has_value()) {
    NEARBY_LOGS(WARNING) << __func__ << ": Device lost during handshake.";
    OnHandshakeFailure(device, PairFailure::kDeviceLostMidPairing);
    return;
  }
  {
    MutexLock lock(&mutex_);
    if (!did_handshake_previously_complete_successfully_map_
            [device.GetModelId()]) {
      for (auto& observer : observers_.GetObservers()) {
        observer->OnHandshakeComplete(device);
      }

      did_handshake_previously_complete_successfully_map_.insert_or_assign(
          device.GetModelId(), true);
    }
    // Reset |num_handshake_attempts_|
    num_handshake_attempts_[device.GetModelId()] = 0;
  }
  StartPairingAttempt(device);
}

void PairerBrokerImpl::OnHandshakeFailure(FastPairDevice& device,
                                          PairFailure failure) {
  NEARBY_LOGS(WARNING) << __func__
                       << ": Handshake failed with device because: " << failure;
  {
    MutexLock lock(&mutex_);
    if (++num_handshake_attempts_[device.GetModelId()] <
        kMaxNumHandshakeAttempts) {
      retry_handshake_timer_ = std::make_unique<TimerImpl>();
      retry_handshake_timer_->Start(
          kRetryHandshakeDelay / absl::Milliseconds(1), 0,
          [&]() { CreateHandshake(device); });
      return;
    }
  }
  NEARBY_LOGS(WARNING)
      << __func__ << ": Handshake failed to be created. Notifying observers.";
  for (auto& observer : observers_.GetObservers()) {
    observer->OnPairFailure(device, failure);
  }
  FastPairHandshakeLookup::GetInstance()->Erase(&device);
}

void PairerBrokerImpl::StartPairingAttempt(FastPairDevice& device) {
  {
    MutexLock lock(&mutex_);
    if (!pair_failure_counts_.contains(device.GetModelId())) {
      pair_failure_counts_[device.GetModelId()] = 0;
    }
  }

  NEARBY_LOGS(INFO) << __func__ << ": " << device;
  MutexLock lock(&mutex_);
  // Create FastPairPairer instance and start pairing.
  fast_pair_pairers_[device.GetModelId()] = FastPairPairerImpl::Factory::Create(
      device, medium_, executor_, account_manager_,
      [&](FastPairDevice& cb_device) {
        executor_->Execute("OnFastPairDevicePaired",
                           [&]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                             OnFastPairDevicePaired(cb_device);
                           });
      },
      [&](FastPairDevice& cb_device, PairFailure failure) {
        executor_->Execute("OnFastPairPairingFailure",
                           [&, failure = std::move(failure)]()
                               ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                                 OnFastPairPairingFailure(cb_device, failure);
                               });
      },
      [&](FastPairDevice& cb_device, PairFailure failure) {
        executor_->Execute("OnAccountKeyFailure",
                           [&, failure = std::move(failure)]()
                               ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                                 OnAccountKeyFailure(cb_device, failure);
                               });
      },
      [&](FastPairDevice& cb_device) {
        executor_->Execute("OnFastPairProcedureComplete",
                           [&]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                             OnFastPairProcedureComplete(cb_device);
                           });
      });
  fast_pair_pairers_[device.GetModelId()]->StartPairing();
}

void PairerBrokerImpl::OnFastPairDevicePaired(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": Device=" << device;
  for (auto& observer : observers_.GetObservers()) {
    observer->OnDevicePaired(device);
  }
  MutexLock lock(&mutex_);
  pair_failure_counts_.erase(device.GetModelId());
}

void PairerBrokerImpl::OnFastPairPairingFailure(FastPairDevice& device,
                                                PairFailure failure) {
  {
    MutexLock lock(&mutex_);
    ++pair_failure_counts_[device.GetModelId()];
    NEARBY_LOGS(INFO) << __func__ << ": Device=" << device
                      << ", Failure=" << failure << ", Failure Count = "
                      << pair_failure_counts_[device.GetModelId()];
    if (pair_failure_counts_[device.GetModelId()] == kMaxFailureRetryCount) {
      if (!fast_pair_pairers_[device.GetModelId()]->IsPaired()) {
        fast_pair_pairers_[device.GetModelId()]->CancelPairing();
      }
      executor_->Execute("EraseHandshakeAndPairers",
                         [&]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                           EraseHandshakeAndPairers(device);
                         });
      NEARBY_LOGS(INFO) << __func__
                        << ": Reached max failure count. Notifying observers.";
      for (auto& observer : observers_.GetObservers()) {
        observer->OnPairFailure(device, failure);
      }
      return;
    }

    if (!fast_pair_pairers_[device.GetModelId()]->IsPaired()) {
      NEARBY_LOGS(INFO) << __func__
                        << ": Cancelling pairing and scheduling retry "
                           "for failed pair attempt.";
      fast_pair_pairers_[device.GetModelId()]->CancelPairing();
      fast_pair_pairers_.erase(device.GetModelId());
      // Create a timer to wait |kCancelPairingRetryDelay| after cancelling
      // pairing to retry the pairing attempt.
      cancel_pairing_timer_ = std::make_unique<TimerImpl>();
      cancel_pairing_timer_->Start(
          kCancelPairingRetryDelay / absl::Milliseconds(1), 0,
          [&]() { PairFastPairDevice(device); });
      return;
    }
    fast_pair_pairers_.erase(device.GetModelId());
  }
  PairFastPairDevice(device);
}

void PairerBrokerImpl::OnAccountKeyFailure(FastPairDevice& device,
                                           PairFailure failure) {
  NEARBY_LOGS(INFO) << __func__ << ": Device=" << device
                    << ", Failure=" << failure;
  executor_->Execute("EraseHandshakeAndPairers",
                     [&]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                       EraseHandshakeAndPairers(device);
                     });
  for (auto& observer : observers_.GetObservers()) {
    observer->OnAccountKeyWrite(device, failure);
  }
}

void PairerBrokerImpl::OnFastPairProcedureComplete(FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__ << ": Device=" << device;
  executor_->Execute("EraseHandshakeAndPairers",
                     [&]() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) {
                       EraseHandshakeAndPairers(device);
                     });
  for (auto& observer : observers_.GetObservers()) {
    observer->OnPairingComplete(device);
  }
  // If we get to this point in the flow for the initial and retroactive
  // pairing scenarios, this means that the account key has successfully
  // been written for devices with a version of V2 or higher.
  if (device.GetVersion().has_value() &&
      device.GetVersion().value() == DeviceFastPairVersion::kHigherThanV1 &&
      device.GetAccountKey().Ok() &&
      (device.GetProtocol() == Protocol::kFastPairInitialPairing ||
       device.GetProtocol() == Protocol::kFastPairRetroactivePairing)) {
    for (auto& observer : observers_.GetObservers()) {
      observer->OnAccountKeyWrite(device, /*error=*/std::nullopt);
    }
  }
}

void PairerBrokerImpl::EraseHandshakeAndPairers(FastPairDevice& device) {
  MutexLock lock(&mutex_);
  NEARBY_LOGS(WARNING) << __func__;
  // |fast_pair_pairers_| and its children objects depend on the handshake
  // instance. Shut them down before destroying the handshake.
  pair_failure_counts_.erase(device.GetModelId());
  fast_pair_pairers_.erase(device.GetModelId());
  FastPairHandshakeLookup::GetInstance()->Erase(&device);
  did_handshake_previously_complete_successfully_map_.insert_or_assign(
      std::string(device.GetModelId()), false);
}

bool PairerBrokerImpl::IsPairing() {
  MutexLock lock(&mutex_);
  // We are guaranteed to not be pairing when the following two maps are
  // empty.
  return !fast_pair_pairers_.empty() || !pair_failure_counts_.empty();
}

void PairerBrokerImpl::StopPairing() {
  MutexLock lock(&mutex_);
  fast_pair_pairers_.clear();
  pair_failure_counts_.clear();
}
}  // namespace fastpair
}  // namespace nearby
