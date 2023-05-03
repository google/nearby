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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_H_

#include <memory>
#include <optional>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/handshake/fast_pair_data_encryptor.h"
#include "fastpair/handshake/fast_pair_gatt_service_client.h"

namespace nearby {
namespace fastpair {

// This class performs the Fast Pair handshake procedure upon creation and
// calls |on_complete| once finished. It also exposes the
// |FastPairDataEncryptor| and |FastPairGattServiceClient| instances that were
// used during the handshake.
//
// The procedure steps are as follows:
//  1. Create a GATT connection to the device.
//  2. Create a data encryptor instance with the generated keys.
//  3. Write the Key-Based Pairing Request to the characteristic
//  (https://developers.google.com/nearby/fast-pair/spec#table1.1)
//  4. Decrypt the response.
//  5. Validate response.
//  6. Set classic address field on |FastPairDevice| instance.
//  7. Complete.

class FastPairHandshake {
 public:
  using OnCompleteCallback = absl::AnyInvocable<void(
      FastPairDevice& device, std::optional<PairFailure> failure)>;

  FastPairHandshake(
      OnCompleteCallback on_complete_cb,
      std::unique_ptr<FastPairDataEncryptor> data_encryptor,
      std::unique_ptr<FastPairGattServiceClient> gatt_service_client)
      : on_complete_callback_(std::move(on_complete_cb)),
        fast_pair_data_encryptor_(std::move(data_encryptor)),
        fast_pair_gatt_service_client_(std::move(gatt_service_client)) {}

  FastPairHandshake(const FastPairHandshake&) = delete;
  FastPairHandshake& operator=(const FastPairHandshake&) = delete;
  virtual ~FastPairHandshake() = default;

  bool completed_successfully() { return completed_successfully_; }

  FastPairDataEncryptor* fast_pair_data_encryptor() {
    return fast_pair_data_encryptor_.get();
  }

  FastPairGattServiceClient* fast_pair_gatt_service_client() {
    return fast_pair_gatt_service_client_.get();
  }

 protected:
  bool completed_successfully_ = false;
  OnCompleteCallback on_complete_callback_;
  std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor_;
  std::unique_ptr<FastPairGattServiceClient> fast_pair_gatt_service_client_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_H_
