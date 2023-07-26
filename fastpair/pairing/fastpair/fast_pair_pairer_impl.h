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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_FASTPAIR_FAST_PAIR_PAIRER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_FASTPAIR_FAST_PAIR_PAIRER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/crypto/decrypted_passkey.h"
#include "fastpair/handshake/fast_pair_gatt_service_client.h"
#include "fastpair/handshake/fast_pair_handshake.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/pairing/fastpair/fast_pair_pairer.h"
#include "internal/account/account_manager.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/timer_impl.h"

namespace nearby {
namespace fastpair {

class FastPairPairerImpl : public FastPairPairer {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairPairer> Create(
        FastPairDevice& device, Mediums& medium, SingleThreadExecutor* executor,
        AccountManager* account_manager, OnPairedCallback on_paired_cb,
        OnPairingFailedCallback on_pair_failed_cb,
        OnAccountKeyFailureCallback on_account_failure_cb,
        OnPairingCompletedCallback on_pairing_completed_cb);

    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory() = default;

    virtual std::unique_ptr<FastPairPairer> CreateInstance(
        FastPairDevice& device, Mediums& medium, SingleThreadExecutor* executor,
        AccountManager* account_manager, OnPairedCallback on_paired_cb,
        OnPairingFailedCallback on_pair_failed_cb,
        OnAccountKeyFailureCallback on_account_failure_cb,
        OnPairingCompletedCallback on_pairing_completed_cb) = 0;

   private:
    static Factory* g_test_factory_;
  };

  FastPairPairerImpl(FastPairDevice& device, Mediums& medium,
                     SingleThreadExecutor* executor,
                     AccountManager* account_manager,
                     OnPairedCallback on_paired_cb,
                     OnPairingFailedCallback on_pair_failed_cb,
                     OnAccountKeyFailureCallback on_account_failure_cb,
                     OnPairingCompletedCallback on_pairing_completed_cb);
  FastPairPairerImpl(const FastPairPairerImpl&) = delete;
  FastPairPairerImpl& operator=(const FastPairPairerImpl&) = delete;
  FastPairPairerImpl(FastPairPairerImpl&&) = delete;
  FastPairPairerImpl& operator=(FastPairPairerImpl&&) = delete;
  bool CancelPairing() override;
  bool IsPaired() override;
  void StartPairing() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_) override;

 private:
  void InitiatePairing();

  void ConfirmPasskey(api::PairingParams pairingParams);
  // FastPairGattServiceClient::WritePasskey callback
  void OnPasskeyResponse(absl::string_view response,
                         std::optional<PairFailure> failure);
  // FastPairDataEncryptor::ParseDecryptedPasskey callback
  void OnParseDecryptedPasskey(std::optional<DecryptedPasskey> passkey);

  // Attempts to write account key to remote device
  void AttemptSendAccountKey() ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  void WriteAccountKey();
  // FastPairDataEncryptor::WriteAccountKey callback
  void OnWriteAccountKey(std::optional<AccountKey> account_key,
                         std::optional<PairFailure> failure);

  // Notify the result of pairing and writing accoutkey.
  void NotifyPaired();
  void NotifyPairingFailed(PairFailure failure);
  void NotifyPairingCompleted();
  void NotifyAccountKeyFailure(PairFailure failure);

  std::string expected_passkey_;
  FastPairHandshake* fast_pair_handshake_;
  FastPairGattServiceClient* fast_pair_gatt_service_client_;
  std::unique_ptr<BluetoothPairing> bluetooth_pairing_;
  FastPairDevice& device_;
  Mediums& mediums_;
  SingleThreadExecutor* executor_;
  AccountManager* account_manager_;
  OnPairedCallback on_paired_cb_ ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  OnPairingFailedCallback on_pair_failed_cb_
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  OnAccountKeyFailureCallback on_account_key_failure_cb_
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  OnPairingCompletedCallback on_pairing_completed_cb_
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(*executor_);
  TimerImpl initiate_pairing_timer_;
  std::unique_ptr<SingleThreadExecutor> pairing_job_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_PAIRING_FASTPAIR_FAST_PAIR_PAIRER_IMPL_H_
