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

#include "internal/crypto_cros/ec_signature_creator.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/types/span.h"
#include "internal/crypto_cros/ec_private_key.h"
#include "internal/crypto_cros/nearby_base.h"
#include "internal/crypto_cros/signature_verifier.h"

namespace nearby {

TEST(ECSignatureCreatorTest, BasicTest) {
  // Do a verify round trip.
  std::unique_ptr<crypto::ECPrivateKey> key_original(
      crypto::ECPrivateKey::Create());
  ASSERT_TRUE(key_original);

  std::vector<uint8_t> key_info;
  ASSERT_TRUE(key_original->ExportPrivateKey(&key_info));

  std::unique_ptr<crypto::ECPrivateKey> key(
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(key_info));
  ASSERT_TRUE(key);
  ASSERT_TRUE(key->key());

  std::unique_ptr<crypto::ECSignatureCreator> signer(
      crypto::ECSignatureCreator::Create(key.get()));
  ASSERT_TRUE(signer);

  std::string data("Hello, World!");
  std::vector<uint8_t> signature;
  ASSERT_TRUE(
      signer->Sign(nearbybase::as_bytes(absl::MakeSpan(data)), &signature));

  std::vector<uint8_t> public_key_info;
  ASSERT_TRUE(key_original->ExportPublicKey(&public_key_info));

  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(crypto::SignatureVerifier::ECDSA_SHA256,
                                  signature, public_key_info));

  verifier.VerifyUpdate(nearbybase::as_bytes(absl::MakeSpan(data)));
  ASSERT_TRUE(verifier.VerifyFinal());
}

}  // namespace nearby
