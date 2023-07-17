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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/handshake/fast_pair_gatt_service_client.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/internal/mediums/robust_gatt_client.h"
#include "internal/platform/mutex.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace fastpair {

// This class is responsible for connecting to the Fast Pair GATT service for a
// device and invoking a callback when ready, or when an error is discovered
// during initialization.
class FastPairGattServiceClientImpl : public FastPairGattServiceClient {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairGattServiceClient> Create(
        const FastPairDevice& device, Mediums& mediums,
        SingleThreadExecutor* executor);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairGattServiceClient> CreateInstance() = 0;

   private:
    static Factory* g_test_factory_;
  };

  explicit FastPairGattServiceClientImpl(const FastPairDevice& device,
                                         Mediums& mediums,
                                         SingleThreadExecutor* executor);
  FastPairGattServiceClientImpl(const FastPairGattServiceClientImpl&) = delete;
  FastPairGattServiceClientImpl& operator=(
      const FastPairGattServiceClientImpl&) = delete;
  ~FastPairGattServiceClientImpl() override = default;

  void InitializeGattConnection(
      absl::AnyInvocable<void(std::optional<PairFailure>)>
          on_gatt_initialized_callback) override;

  void WriteRequestAsync(
      uint8_t message_type, uint8_t flags, absl::string_view provider_address,
      absl::string_view seekers_address,
      const FastPairDataEncryptor& fast_pair_data_encryptor,
      WriteResponseCallback write_response_callback) override;

  void WritePasskeyAsync(
      uint8_t message_type, uint32_t passkey,
      const FastPairDataEncryptor& fast_pair_data_encryptor,
      WriteResponseCallback write_response_callback) override;

  void WriteAccountKey(
      const FastPairDataEncryptor& fast_pair_data_encryptor,
      WriteAccountkeyCallback write_accountkey_callback) override;

  // Allows tests to modify GATT operation timeouts.
  RobustGattClient::ConnectionParams& GetConnectionParams() {
    return gatt_connection_params_;
  }

 private:
  // Attempt to create a GATT connection with the device. This method may be
  // called multiple times.
  void AttemptGattConnection();
  void CreateGattConnection();

  // Operations on KeyBased Characteristic
  // Creates a data vector based on parameter information.
  std::array<uint8_t, kAesBlockByteSize> CreateRequest(
      uint8_t message_type, uint8_t flags, absl::string_view provider_address,
      absl::string_view seekers_address);
  // Write request to KeyBased Characteristic
  void WriteKeyBasedCharacteristic(absl::string_view request);

  // Operations on Passkey Characteristic
  // Creates a data vector based on parameter information.
  std::array<uint8_t, kAesBlockByteSize> CreatePasskeyBlock(
      uint8_t message_type, uint32_t passkey);
  // Write request to Passkey Characteristic
  void WritePasskeyCharacteristic(absl::string_view request);

  // Operations on Account Key Characteristic
  // Creates an Account key.
  std::array<uint8_t, kAesBlockByteSize> CreateAccountKeyBlock();

  // Invokes the initialized callback with the proper PairFailure and clears
  // local state.
  void NotifyInitializedError(PairFailure failure);
  // Invokes the write response callback with the proper PairFailure on a
  // write error.
  void NotifyWriteRequestError(PairFailure failure) {
    NotifyWriteRequestResult("", failure);
  }
  void NotifyWriteRequestResult(absl::string_view value,
                                std::optional<PairFailure> = std::nullopt);
  void NotifyWritePasskeyError(PairFailure failure) {
    NotifyWritePasskeyResult("", failure);
  }
  void NotifyWritePasskeyResult(absl::string_view value,
                                std::optional<PairFailure> = std::nullopt);
  void NotifyWriteAccountKeyError(PairFailure failure) {
    NotifyWriteAccountKeyResult(std::nullopt, failure);
  }
  void NotifyWriteAccountKeyResult(
      std::optional<AccountKey> account_key,
      std::optional<PairFailure> failure = std::nullopt);

  void ClearCurrentState();

  // Timers
  TimerImpl passkey_subscription_timer_;
  TimerImpl key_based_write_request_timer_;
  TimerImpl passkey_write_request_timer_;
  TimerImpl account_key_write_request_timer_;

  void OnGattServiceDiscoveryTimeout();

  // Callback
  absl::AnyInvocable<void(std::optional<PairFailure>)>
      on_gatt_initialized_callback_;
  WriteResponseCallback key_based_write_response_callback_;
  WriteResponseCallback passkey_write_response_callback_;
  WriteAccountkeyCallback account_key_write_callback_;

  Mutex mutex_;
  bool is_initialized_ = false;
  std::string device_address_;
  std::unique_ptr<RobustGattClient> gatt_client_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<RobustGattClient> defunct_gatt_client_;
  RobustGattClient::ConnectionParams gatt_connection_params_;
  Mediums& mediums_;
  SingleThreadExecutor* executor_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_
