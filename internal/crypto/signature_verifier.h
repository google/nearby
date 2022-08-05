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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SIGNATURE_VERIFIER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SIGNATURE_VERIFIER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "internal/crypto/crypto_export.h"

namespace crypto {

// The SignatureVerifier class verifies a signature using a bare public key
// (as opposed to a certificate).
class CRYPTO_EXPORT SignatureVerifier {
 public:
  // The set of supported signature algorithms. Extend as required.
  enum SignatureAlgorithm {
    RSA_PKCS1_SHA1,
    RSA_PKCS1_SHA256,
    ECDSA_SHA256,
    // This is RSA-PSS with SHA-256 as both signing hash and MGF-1 hash, and the
    // salt length matching the hash length.
    RSA_PSS_SHA256,
  };

  SignatureVerifier();
  ~SignatureVerifier();

  // Streaming interface:

  // Initiates a signature verification operation.  This should be followed
  // by one or more VerifyUpdate calls and a VerifyFinal call.
  //
  // The signature is encoded according to the signature algorithm.
  //
  // The public key is specified as a DER encoded ASN.1 SubjectPublicKeyInfo
  // structure, which contains not only the public key but also its type
  // (algorithm):
  //   SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //       algorithm            AlgorithmIdentifier,
  //       subjectPublicKey     BIT STRING  }
  bool VerifyInit(SignatureAlgorithm signature_algorithm,
                  absl::Span<const uint8_t> signature,
                  absl::Span<const uint8_t> public_key_info);

  // Feeds a piece of the data to the signature verifier.
  void VerifyUpdate(absl::Span<const uint8_t> data_part);

  // Concludes a signature verification operation.  Returns true if the
  // signature is valid.  Returns false if the signature is invalid or an
  // error occurred.
  bool VerifyFinal();

 private:
  void Reset();

  std::vector<uint8_t> signature_;

  struct VerifyContext;
  std::unique_ptr<VerifyContext> verify_context_;
};

}  // namespace crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SIGNATURE_VERIFIER_H_
