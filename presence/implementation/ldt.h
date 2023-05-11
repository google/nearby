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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_LDT_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_LDT_H_

#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#ifdef USE_RUST_LDT
#include "third_party/beto-core/src/nearby/presence/ldt_np_adv_ffi/include/np_ldt.h"
#else
#include "presence/implementation/np_ldt.h"
#endif /* USE_RUST_LDT */

namespace nearby {
namespace presence {

// C++ abstraction on top of LDT C API.
class LdtEncryptor {
 public:
  LdtEncryptor(const LdtEncryptor&) = delete;
  LdtEncryptor(LdtEncryptor&& other);
  LdtEncryptor& operator=(const LdtEncryptor&) = delete;
  LdtEncryptor& operator=(LdtEncryptor&& other) {
    std::swap(ldt_encrypt_handle_, other.ldt_encrypt_handle_);
    std::swap(ldt_decrypt_handle_, other.ldt_decrypt_handle_);
    return *this;
  }
  ~LdtEncryptor();

  // Creates an instance of `LdtEncryptor`.
  // `key_seed` is used to generate LDT encryption and decryption keys.
  // `known_hmac` is used during decryption to verify if the message was
  // encrypted with the expected key.
  static absl::StatusOr<LdtEncryptor> Create(absl::string_view key_seed,
                                             absl::string_view known_hmac);

  // Encrypts `data`, which must be 16 - 31 bytes long.
  absl::StatusOr<std::string> Encrypt(absl::string_view data,
                                      absl::string_view salt);

  // Decrypts `data` and verifies if it was encrypted with a key generated from
  // `key_seed`.
  absl::StatusOr<std::string> DecryptAndVerify(absl::string_view data,
                                               absl::string_view salt);

 private:
  explicit LdtEncryptor(NpLdtEncryptHandle ldt_encrypt_handle,
                        NpLdtDecryptHandle ldt_decrypt_handle)
      : ldt_encrypt_handle_(ldt_encrypt_handle),
        ldt_decrypt_handle_(ldt_decrypt_handle) {}
  // An opaque handle to the underlying LDT implementation. It can be null iff
  // this object has already been destroyed.
  NpLdtEncryptHandle ldt_encrypt_handle_;
  NpLdtDecryptHandle ldt_decrypt_handle_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_LDT_H_
