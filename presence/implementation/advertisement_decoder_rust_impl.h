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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_DECODER_RUST_IMPL_H_
#define THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_DECODER_RUST_IMPL_H_

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "nearby_protocol.h"
#include "presence/implementation/advertisement_decoder.h"

namespace nearby {
namespace presence {

// Implements the Rust backed parsing and decrypting of advertisement bytes
class AdvertisementDecoderImpl : public AdvertisementDecoder {
 public:
  AdvertisementDecoderImpl()
      : cred_book_(InitializeCredentialBook(nullptr)),
        private_credentials_(
            std::vector<::nearby::internal::SharedCredential>()) {}

  explicit AdvertisementDecoderImpl(
      absl::flat_hash_map<nearby::internal::IdentityType,
                          std::vector<internal::SharedCredential>>*
          credentials_map)
      : cred_book_(InitializeCredentialBook(credentials_map)),
        private_credentials_(
            (*credentials_map)
                [internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP]) {}

  absl::StatusOr<Advertisement> DecodeAdvertisement(
      absl::string_view advertisement) override;

 private:
  nearby_protocol::CredentialBook InitializeCredentialBook(
      absl::flat_hash_map<nearby::internal::IdentityType,
                          std::vector<::nearby::internal::SharedCredential>>*
          credentials_map);
  nearby_protocol::CredentialBook cred_book_;
  std::vector<::nearby::internal::SharedCredential> private_credentials_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_DECODER_IMPL_H_
