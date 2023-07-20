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

#include "internal/crypto_cros/signature_verifier.h"

#include <memory>

#ifdef NEARBY_CHROMIUM
#include "internal/platform/logging.h"
#elif defined(NEARBY_SWIFTPM)
#include "internal/platform/logging.h"
#else
#include "absl/log/check.h"  // nogncheck
#endif

#include "internal/crypto_cros/openssl_util.h"
#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

namespace crypto {

struct SignatureVerifier::VerifyContext {
  bssl::ScopedEVP_MD_CTX ctx;
};

SignatureVerifier::SignatureVerifier() = default;

SignatureVerifier::~SignatureVerifier() = default;

bool SignatureVerifier::VerifyInit(SignatureAlgorithm signature_algorithm,
                                   absl::Span<const uint8_t> signature,
                                   absl::Span<const uint8_t> public_key_info) {
  OpenSSLErrStackTracer err_tracer;

  int pkey_type = EVP_PKEY_NONE;
  const EVP_MD* digest = nullptr;
  switch (signature_algorithm) {
    case RSA_PKCS1_SHA1:
      pkey_type = EVP_PKEY_RSA;
      digest = EVP_sha1();
      break;
    case RSA_PKCS1_SHA256:
    case RSA_PSS_SHA256:
      pkey_type = EVP_PKEY_RSA;
      digest = EVP_sha256();
      break;
    case ECDSA_SHA256:
      pkey_type = EVP_PKEY_EC;
      digest = EVP_sha256();
      break;
  }
  DCHECK_NE(EVP_PKEY_NONE, pkey_type);
  DCHECK(digest);

  if (verify_context_) return false;

  verify_context_ = std::make_unique<VerifyContext>();
  signature_.assign(signature.data(), signature.data() + signature.size());

  CBS cbs;
  CBS_init(&cbs, public_key_info.data(), public_key_info.size());
  bssl::UniquePtr<EVP_PKEY> public_key(EVP_parse_public_key(&cbs));
  if (!public_key || CBS_len(&cbs) != 0 ||
      EVP_PKEY_id(public_key.get()) != pkey_type) {
    return false;
  }

  EVP_PKEY_CTX* pkey_ctx;
  if (!EVP_DigestVerifyInit(verify_context_->ctx.get(), &pkey_ctx, digest,
                            nullptr, public_key.get())) {
    return false;
  }

  if (signature_algorithm == RSA_PSS_SHA256) {
    if (!EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) ||
        !EVP_PKEY_CTX_set_rsa_mgf1_md(pkey_ctx, digest) ||
        !EVP_PKEY_CTX_set_rsa_pss_saltlen(
            pkey_ctx, -1 /* match digest and salt length */)) {
      return false;
    }
  }

  return true;
}

void SignatureVerifier::VerifyUpdate(absl::Span<const uint8_t> data_part) {
  DCHECK(verify_context_);
  OpenSSLErrStackTracer err_tracer;
  int rv = EVP_DigestVerifyUpdate(verify_context_->ctx.get(), data_part.data(),
                                  data_part.size());
  DCHECK_EQ(rv, 1);
}

bool SignatureVerifier::VerifyFinal() {
  DCHECK(verify_context_);
  OpenSSLErrStackTracer err_tracer;
  int rv = EVP_DigestVerifyFinal(verify_context_->ctx.get(), signature_.data(),
                                 signature_.size());
  DCHECK_EQ(static_cast<int>(!!rv), rv);
  Reset();
  return rv == 1;
}

void SignatureVerifier::Reset() {
  verify_context_.reset();
  signature_.clear();
}

}  // namespace crypto
