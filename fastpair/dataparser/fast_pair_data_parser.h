// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_DATAPARSER_FAST_PAIR_DATA_PARSER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_DATAPARSER_FAST_PAIR_DATA_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "fastpair/crypto/decrypted_passkey.h"
#include "fastpair/crypto/decrypted_response.h"

namespace nearby {
namespace fastpair {

struct GetHexModelIdFromServiceDataCallback {
  absl::AnyInvocable<void(std::optional<absl::string_view>)> on_retrieved_cb =
      [](std::optional<absl::string_view>) {};
};

struct ParseDecryptResponseCallback {
  absl::AnyInvocable<void(std::optional<DecryptedResponse>)> on_decrypted_cb =
      [](std::optional<DecryptedResponse>) {};
};

struct ParseDecryptPasskeyCallback {
  absl::AnyInvocable<void(std::optional<DecryptedPasskey>)> on_decrypted_cb =
      [](std::optional<DecryptedPasskey>) {};
};

// This class is responsible for parsing the untrusted bytes for Fast Pair.
class FastPairDataParser {
 public:
  // Gets the hex string representation of the device's model ID from the
  // service data.
  static void GetHexModelIdFromServiceData(
      const std::vector<uint8_t>& fast_pair_service_data,
      GetHexModelIdFromServiceDataCallback callback);

  // Decrypts |encrypted_response_bytes| using |aes_key| and returns a parsed
  // DecryptedResponse instance if possible.
  static void ParseDecryptedResponse(
      const std::vector<uint8_t>& aes_key_bytes,
      const std::vector<uint8_t>& encrypted_response_bytes,
      ParseDecryptResponseCallback callback);

  // Decrypts |encrypted_passkey_bytes| using |aes_key| and returns a parsed
  // DecryptedPasskey instance if possible.
  static void ParseDecryptedPasskey(
      const std::vector<uint8_t>& aes_key_bytes,
      const std::vector<uint8_t>& encrypted_passkey_bytes,
      ParseDecryptPasskeyCallback callback);
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_DATAPARSER_FAST_PAIR_DATA_PARSER_H_
