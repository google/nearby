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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_ED25519_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_ED25519_H_

#include <cstring>
#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "internal/crypto_cros/crypto_export.h"
#include <openssl/base.h>
#include <openssl/digest.h>
#include <openssl/evp.h>

namespace crypto {

#ifdef OPENSSL_IS_BORINGSSL
using CryptoKeyUniquePtr = ::bssl::UniquePtr<EVP_PKEY>;
using CryptoMdCtxUniquePtr = ::bssl::UniquePtr<EVP_MD_CTX>;
#else
struct Ed25519KeyFree {
  void operator()(EVP_PKEY* pkey) const { EVP_PKEY_free(pkey); }
};

struct Ed25519MdCtxFree {
  void operator()(EVP_MD_CTX* ctx) const { EVP_MD_CTX_free(ctx); }
};
using CryptoKeyUniquePtr = std::unique_ptr<EVP_PKEY, Ed25519KeyFree>;
using CryptoMdCtxUniquePtr = std::unique_ptr<EVP_MD_CTX, Ed25519MdCtxFree>;
#endif

struct Ed25519KeyPair {
  ~Ed25519KeyPair();
  std::string private_key;
  std::string public_key;
};

class CRYPTO_EXPORT Ed25519Signer {
 public:
  static absl::StatusOr<Ed25519Signer> Create(std::string private_key);
  static absl::StatusOr<Ed25519KeyPair> CreateNewKeyPair(
      absl::string_view key_seed);
  static absl::StatusOr<Ed25519KeyPair> CreateNewKeyPair();
  std::optional<std::string> Sign(absl::string_view data);

 private:
  explicit Ed25519Signer(CryptoKeyUniquePtr private_key);

  CryptoKeyUniquePtr private_key_;
};

class CRYPTO_EXPORT Ed25519Verifier {
 public:
  static absl::StatusOr<Ed25519Verifier> Create(std::string public_key);
  absl::Status Verify(absl::string_view data, absl::string_view signature);

 private:
  explicit Ed25519Verifier(CryptoKeyUniquePtr public_key);

  CryptoKeyUniquePtr public_key_;
};

}  // namespace crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_ED25519_H_
