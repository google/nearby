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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_

#include <array>
#include <optional>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/account_key.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/handshake/fast_pair_data_encryptor.h"
#include "internal/platform/ble_v2.h"

namespace nearby {
namespace fastpair {
using WriteResponseCallback = absl::AnyInvocable<void(
    absl::string_view value, std::optional<PairFailure> failure)>;
using WriteAccountkeyCallback = absl::AnyInvocable<void(
    std::optional<AccountKey> account_key, std::optional<PairFailure> failure)>;

// This class is responsible for connecting to the Fast Pair GATT service for a
// device and invoking a callback when ready, or when an error is discovered
// during initialization.
class FastPairGattServiceClient {
 public:
  virtual ~FastPairGattServiceClient() = default;

  virtual void InitializeGattConnection(
      absl::AnyInvocable<void(std::optional<PairFailure>)>
          on_gatt_initialized_callback) = 0;

  // Constructs a data vector based on the message type, flags, provider
  // address, and seekers address.
  // Subscribe a notification for key based Pairing.
  // Once the notification subscribed successfully, the message data will be
  // written to the key based characteristic.
  virtual void WriteRequestAsync(
      uint8_t message_type, uint8_t flags, absl::string_view provider_address,
      absl::string_view seekers_address,
      const FastPairDataEncryptor& fast_pair_data_encryptor,
      WriteResponseCallback write_response_callback) = 0;

  // Constructs a data vector based on the message type and passkey.
  // Subscribe a notification for the passkey.
  // Once the notification subscribed successfully, the passkey data will be
  // written to the passkey characteristic.
  virtual void WritePasskeyAsync(
      uint8_t message_type, uint32_t passkey,
      const FastPairDataEncryptor& fast_pair_data_encryptor,
      WriteResponseCallback write_response_callback) = 0;

  // Writes the account key to the account key characteristic.
  virtual void WriteAccountKey(
      const FastPairDataEncryptor& fast_pair_data_encryptor,
      WriteAccountkeyCallback write_accountkey_callback) = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_H_
