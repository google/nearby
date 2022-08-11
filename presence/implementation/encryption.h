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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ENCRYPTION_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ENCRYPTION_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace nearby {
namespace presence {

/** Utilities for encryption and decryption of presence advertisements. */
class Encryption {
 public:
  /**
   * Uses a HMAC based HKDF algorithm to generate and return a
   * new byte array of given length based on input bytes and a zero-initialized
   * salt.
   */
  static std::string CustomizeBytesSize(absl::string_view bytes, size_t len);

  /** Generates a random byte array of given size. */
  static std::string GenerateRandomByteArray(size_t len);

  /** Generates encrypted metadata key. `metadata_encryption_key` must be 14 or
   * 16 bytes long. `authenticity_key` is the AES key and must be 128 bit (16
   * bytes) long. `salt` must be 2 bytes long.
   *
   * Note, because the salt is ony 16 bit long, the caller should not reuse
   * salts. Reusing salts will expose the encrypted metadata key. The solution
   * in NP is to store all used salts and rotate the autheticity key when we run
   * out of salts.
   */
  static absl::StatusOr<std::string> GenerateEncryptedMetadataKey(
      absl::string_view metadata_encryption_key,
      absl::string_view authenticity_key, absl::string_view salt) {
    return RunMetadataEncryption(metadata_encryption_key, authenticity_key,
                                 salt, /*encrypt= */ true);
  }

  /** Generates decrypted metadata key. `encrypted_metadata_key` must be 14 or
   * 16 bytes long. `authenticity_key` is the AES key and must be 128 bit (16
   * bytes) long. `salt` must be 2 bytes long.*/
  static absl::StatusOr<std::string> GenerateDecryptedMetadataKey(
      absl::string_view encrypted_metadata_key,
      absl::string_view authenticity_key, absl::string_view salt) {
    return RunMetadataEncryption(encrypted_metadata_key, authenticity_key, salt,
                                 /*encrypt= */ false);
  }

 private:
  static absl::StatusOr<std::string> RunMetadataEncryption(
      absl::string_view metadata, absl::string_view key, absl::string_view salt,
      bool encrypt);
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ENCRYPTION_H_
