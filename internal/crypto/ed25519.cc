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

#include "internal/crypto/ed25519.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "internal/platform/crypto.h"
#include <openssl/base.h>
#include <openssl/evp.h>

namespace crypto {

constexpr size_t kEd25519SignatureSize = 64;
constexpr size_t kEd25519PrivateKeySize = 32;
constexpr size_t kEd25519PublicKeySize = 32;
constexpr size_t kEd25519KeySeedSize = 32;

// Keypair
Ed25519KeyPair::~Ed25519KeyPair() {
  memset(private_key.data(), 0, private_key.size());
  memset(public_key.data(), 0, public_key.size());
}

// Signer
absl::StatusOr<Ed25519Signer> Ed25519Signer::Create(std::string private_key) {
  // OpenSSL/BoringSSL consider the ED25519's private key to be: private_key ||
  // public_key.
  const size_t kKeyLength = kEd25519PrivateKeySize + kEd25519PublicKeySize;
  if (private_key.length() != kKeyLength) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Only acceptable key length is %d", kKeyLength));
  }
  CryptoKeyUniquePtr priv_key(EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, /*unused=*/nullptr,
      reinterpret_cast<const uint8_t *>(private_key.data()),
      kEd25519PrivateKeySize));
  if (priv_key == nullptr) {
    return absl::InternalError("EVP_PKEY_new_raw_private_key failed");
  }
  return Ed25519Signer(std::move(priv_key));
}

absl::StatusOr<Ed25519KeyPair> Ed25519Signer::CreateNewKeyPair(
    absl::string_view key_seed) {
  if (key_seed.length() != kEd25519KeySeedSize) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Only acceptable key seed length is %d", kEd25519KeySeedSize));
  }
  CryptoKeyUniquePtr priv_key(EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, /*unused=*/nullptr,
      reinterpret_cast<const uint8_t *>(key_seed.data()), kEd25519KeySeedSize));
  if (priv_key == nullptr) {
    return absl::InternalError("EVP_PKEY_new_raw_private_key failed");
  }
  Ed25519KeyPair key_pair;
  key_pair.private_key.resize(kEd25519PrivateKeySize);
  key_pair.public_key.resize(kEd25519PublicKeySize);
  uint8_t *priv_key_ptr = reinterpret_cast<uint8_t *>(&key_pair.private_key[0]);
  uint8_t *pub_key_ptr = reinterpret_cast<uint8_t *>(&key_pair.public_key[0]);
  size_t len = kEd25519PrivateKeySize;
  if (EVP_PKEY_get_raw_private_key(priv_key.get(), priv_key_ptr, &len) != 1) {
    return absl::InternalError("EVP_PKEY_get_raw_private_key failed");
  }
  if (len != kEd25519PrivateKeySize) {
    return absl::InternalError(
        absl::StrCat("Invalid private key size; expected ",
                     kEd25519PrivateKeySize, " got ", len));
  }
  len = kEd25519PublicKeySize;
  if (EVP_PKEY_get_raw_public_key(priv_key.get(), pub_key_ptr, &len) != 1) {
    return absl::InternalError("EVP_PKEY_get_raw_public_key failed");
  }
  if (len != kEd25519PublicKeySize) {
    return absl::InternalError(
        absl::StrCat("Invalid public key size; expected ",
                     kEd25519PublicKeySize, " got ", len));
  }
  return key_pair;
}

absl::StatusOr<Ed25519KeyPair> Ed25519Signer::CreateNewKeyPair() {
  uint8_t key_seed[kEd25519KeySeedSize] = {0};
  nearby::RandBytes(key_seed, kEd25519KeySeedSize);
  return CreateNewKeyPair(
      std::string(reinterpret_cast<char *>(key_seed), kEd25519KeySeedSize));
}

Ed25519Signer::Ed25519Signer(CryptoKeyUniquePtr private_key) {
  private_key_ = std::move(private_key);
}

std::optional<std::string> Ed25519Signer::Sign(absl::string_view data) {
  if (data.empty() || data.data() == nullptr) {
    data = "";
  }
  uint8_t signature[kEd25519SignatureSize] = {0};
  CryptoMdCtxUniquePtr md_ctx(EVP_MD_CTX_create());
  if (md_ctx == nullptr) {
    return std::nullopt;
  }
  size_t signature_size = kEd25519SignatureSize;
  if (EVP_DigestSignInit(md_ctx.get(), /*pctx=*/nullptr, /*type=*/nullptr,
                         /*e=*/nullptr, private_key_.get()) != 1 ||
      EVP_DigestSign(md_ctx.get(), signature, &signature_size,
                     /*data=*/reinterpret_cast<const uint8_t *>(data.data()),
                     data.size()) != 1) {
    return std::nullopt;
  }
  return std::string(reinterpret_cast<char *>(signature),
                     kEd25519SignatureSize);
}

// Verifier
absl::StatusOr<Ed25519Verifier> Ed25519Verifier::Create(
    std::string public_key) {
  if (public_key.length() != kEd25519PublicKeySize) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Only acceptable key length is %d", kEd25519PublicKeySize));
  }
  CryptoKeyUniquePtr pub_key(EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, /*unused=*/nullptr,
      reinterpret_cast<const uint8_t *>(public_key.data()),
      kEd25519PublicKeySize));
  if (pub_key == nullptr) {
    return absl::InternalError("EVP_PKEY_new_raw_public_key failed");
  }
  return Ed25519Verifier(std::move(pub_key));
}

Ed25519Verifier::Ed25519Verifier(CryptoKeyUniquePtr public_key) {
  public_key_ = std::move(public_key);
}

absl::Status Ed25519Verifier::Verify(absl::string_view data,
                                     absl::string_view signature) {
  if (data.empty() || data.data() == nullptr) {
    data = "";
  }
  if (signature.empty() || signature.data() == nullptr) {
    signature = "";
  }
  if (signature.length() != kEd25519SignatureSize) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Only acceptable signature length is %d", kEd25519SignatureSize));
  }
  CryptoMdCtxUniquePtr md_ctx(EVP_MD_CTX_create());
  if (md_ctx == nullptr) {
    return absl::InternalError("EVP_MD_CTX_create failed");
  }
  // `type` must be set to nullptr with Ed25519.
  if (EVP_DigestVerifyInit(md_ctx.get(), /*pctx=*/nullptr, /*type=*/nullptr,
                           /*e=*/nullptr, public_key_.get()) != 1) {
    return absl::InternalError("EVP_DigestVerifyInit failed");
  }

  return EVP_DigestVerify(
             md_ctx.get(),
             /*sig=*/reinterpret_cast<const uint8_t *>(signature.data()),
             signature.size(),
             /*data=*/reinterpret_cast<const uint8_t *>(data.data()),
             data.size()) != 0
             ? absl::OkStatus()
             : absl::InternalError("Signature is invalid.");
}

}  // namespace crypto
