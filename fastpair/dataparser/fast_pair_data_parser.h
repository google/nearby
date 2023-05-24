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
#include "fastpair/common/non_discoverable_advertisement.h"
#include "fastpair/crypto/decrypted_passkey.h"
#include "fastpair/crypto/decrypted_response.h"

namespace nearby {
namespace fastpair {



// This class is responsible for parsing the untrusted bytes for Fast Pair.
class FastPairDataParser {
  using GetHexModelIdFromServiceDataCallback =
      absl::AnyInvocable<void(std::optional<absl::string_view>)>;

  using ParseDecryptResponseCallback =
      absl::AnyInvocable<void(std::optional<DecryptedResponse>)>;

  using ParseDecryptPasskeyCallback =
      absl::AnyInvocable<void(std::optional<DecryptedPasskey>)>;

  using ParseNotDiscoverableAdvertisementCallback =
      absl::AnyInvocable<void(std::optional<NonDiscoverableAdvertisement>)>;

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

  // Parses a 'Non Discoverable' advertisement from |service_data|.
  // If the advertisement does not contain information about salt, use the
  // |address| as salt instead.
  static void ParseNotDiscoverableAdvertisement(
      absl::string_view fast_pair_service_data, absl::string_view address,
      ParseNotDiscoverableAdvertisementCallback callback);
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_DATAPARSER_FAST_PAIR_DATA_PARSER_H_
