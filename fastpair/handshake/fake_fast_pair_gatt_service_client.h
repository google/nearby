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
#ifndef THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_

#include <optional>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/handshake/fast_pair_gatt_service_client.h"

namespace nearby {
namespace fastpair {

// This class fakes FastPairGattServiceClient and permits setting which
// PairFailure, if any, is run with the callback.
class FakeFastPairGattServiceClient : public FastPairGattServiceClient {
 public:
  void InitializeGattConnection(
      absl::AnyInvocable<void(std::optional<PairFailure>)>
          on_gatt_initialized_callback) override {
    on_initialized_callback_ = std::move(on_gatt_initialized_callback);
  }

  void WriteRequestAsync(
      uint8_t message_type, uint8_t flags, absl::string_view provider_address,
      absl::string_view seekers_address,
      const FastPairDataEncryptor& fast_pair_data_encryptor,
      WriteResponseCallback write_response_callback) override {
    key_based_write_response_callback_ = std::move(write_response_callback);
  }

  void WritePasskeyAsync(
      uint8_t message_type, uint32_t passkey,
      const FastPairDataEncryptor& fast_pair_data_encryptor,
      WriteResponseCallback write_response_callback) override {
    passkey_write_response_callback_ = std::move(write_response_callback);
  }

  void RunOnGattClientInitializedCallback(
      std::optional<PairFailure> failure = std::nullopt) {
    std::move(on_initialized_callback_)(failure);
  }

  void RunWriteResponseCallback(
      absl::string_view value,
      std::optional<PairFailure> failure = std::nullopt) {
    std::move(key_based_write_response_callback_)(value, failure);
  }

  void RunWritePasskeyCallback(
      absl::string_view value,
      std::optional<PairFailure> failure = std::nullopt) {
    std::move(passkey_write_response_callback_)(value, failure);
  }

 private:
  absl::AnyInvocable<void(std::optional<PairFailure>)> on_initialized_callback_;
  WriteResponseCallback key_based_write_response_callback_;
  WriteResponseCallback passkey_write_response_callback_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
