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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_EC_SIGNATURE_CREATOR_IMPL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_EC_SIGNATURE_CREATOR_IMPL_H_

#include <stdint.h>

#include <vector>

#include "absl/types/span.h"
#include "internal/crypto_cros/ec_signature_creator.h"

namespace nearby::crypto {

class ECSignatureCreatorImpl : public ECSignatureCreator {
 public:
  explicit ECSignatureCreatorImpl(ECPrivateKey* key);

  ECSignatureCreatorImpl(const ECSignatureCreatorImpl&) = delete;
  ECSignatureCreatorImpl& operator=(const ECSignatureCreatorImpl&) = delete;

  ~ECSignatureCreatorImpl() override;

  bool Sign(absl::Span<const uint8_t> data,
            std::vector<uint8_t>* signature) override;

  bool DecodeSignature(const std::vector<uint8_t>& der_sig,
                       std::vector<uint8_t>* out_raw_sig) override;

 private:
  ECPrivateKey* key_;
};

}  // namespace nearby::crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_EC_SIGNATURE_CREATOR_IMPL_H_
