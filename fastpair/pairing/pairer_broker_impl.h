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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_PAIRER_BROKER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_PAIRER_BROKER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/pairing/fastpair/fast_pair_pairer.h"
#include "fastpair/pairing/pairer_broker.h"
#include "internal/account/account_manager.h"
#include "internal/base/observer_list.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/timer_impl.h"

namespace nearby {
namespace fastpair {

class PairerBrokerImpl : public PairerBroker {
 public:
  explicit PairerBrokerImpl(Mediums& medium, SingleThreadExecutor* executor,
                            AccountManager* account_manager);
  PairerBrokerImpl(const PairerBrokerImpl&) = delete;
  PairerBrokerImpl& operator=(const PairerBrokerImpl&) = delete;

  // PairingBroker:
  void AddObserver(Observer* observer) override;
  // The `observer` can still be called after removing if it has already
  // been scheduled to run (or is running) on the background thread.
  void RemoveObserver(Observer* observer) override;
  void PairDevice(FastPairDevice& device) override;
  bool IsPairing() override;
  void StopPairing() override;

  friend class PairerBrokerImplTest;

 private:
  mutable Mutex mutex_;
  void PairFastPairDevice(FastPairDevice& device);
  void CreateHandshake(FastPairDevice& device);
  void OnHandshakeComplete(FastPairDevice& device,
                           std::optional<PairFailure> failure)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void OnHandshakeFailure(FastPairDevice& device, PairFailure failure)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);

  void StartPairingAttempt(FastPairDevice& device);

  void OnFastPairDevicePaired(FastPairDevice& device)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void OnFastPairPairingFailure(FastPairDevice& device, PairFailure failure)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void OnFastPairProcedureComplete(FastPairDevice& device)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void OnAccountKeyFailure(FastPairDevice& device, PairFailure failure)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);

  void EraseHandshakeAndPairers(FastPairDevice& device)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);

  Mediums& medium_;
  SingleThreadExecutor* executor_;
  AccountManager* account_manager_;

  // The key for all the following maps is a device model id.
  absl::flat_hash_map<std::string, std::unique_ptr<FastPairPairer>>
      fast_pair_pairers_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, int> pair_failure_counts_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, bool>
      did_handshake_previously_complete_successfully_map_
          ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, int> num_handshake_attempts_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, std::string>
      model_id_to_current_ble_address_map_ ABSL_GUARDED_BY(mutex_);

  // Timer
  std::unique_ptr<TimerImpl> cancel_pairing_timer_;
  std::unique_ptr<TimerImpl> retry_handshake_timer_;

  ObserverList<Observer> observers_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_PAIRER_BROKER_IMPL_H_
