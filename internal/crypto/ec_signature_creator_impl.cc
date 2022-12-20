// Copyright 2020 Google LLC
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

#include "internal/crypto/ec_signature_creator_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "internal/crypto/ec_private_key.h"
#include "internal/crypto/openssl_util.h"
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

namespace crypto {

ECSignatureCreatorImpl::ECSignatureCreatorImpl(ECPrivateKey* key) : key_(key) {
  EnsureOpenSSLInit();
}

ECSignatureCreatorImpl::~ECSignatureCreatorImpl() = default;

bool ECSignatureCreatorImpl::Sign(absl::Span<const uint8_t> data,
                                  std::vector<uint8_t>* signature) {
  OpenSSLErrStackTracer err_tracer;
  bssl::ScopedEVP_MD_CTX ctx;
  size_t sig_len = 0;
  if (!ctx.get() ||
      !EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr,
                          key_->key()) ||
      !EVP_DigestSignUpdate(ctx.get(), data.data(), data.size()) ||
      !EVP_DigestSignFinal(ctx.get(), nullptr, &sig_len)) {
    return false;
  }

  signature->resize(sig_len);
  if (!EVP_DigestSignFinal(ctx.get(), &signature->front(), &sig_len))
    return false;

  // NOTE: A call to EVP_DigestSignFinal() with a nullptr second parameter
  // returns a maximum allocation size, while the call without a nullptr
  // returns the real one, which may be smaller.
  signature->resize(sig_len);
  return true;
}

bool ECSignatureCreatorImpl::DecodeSignature(
    const std::vector<uint8_t>& der_sig, std::vector<uint8_t>* out_raw_sig) {
  OpenSSLErrStackTracer err_tracer;
  // Create ECDSA_SIG object from DER-encoded data.
  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(
      ECDSA_SIG_from_bytes(der_sig.data(), der_sig.size()));
  if (!ecdsa_sig.get()) return false;

  // The result is made of two 32-byte vectors.
  const size_t kMaxBytesPerBN = 32;
  std::vector<uint8_t> result(2 * kMaxBytesPerBN);

  if (!BN_bn2bin_padded(&result[0], kMaxBytesPerBN, ecdsa_sig->r) ||
      !BN_bn2bin_padded(&result[kMaxBytesPerBN], kMaxBytesPerBN,
                        ecdsa_sig->s)) {
    return false;
  }
  out_raw_sig->swap(result);
  return true;
}

}  // namespace crypto
