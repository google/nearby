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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_DECODER_IMPL_H_
#define THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_DECODER_IMPL_H_

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/proto/credential.pb.h"
#include "presence/implementation/advertisement_decoder.h"

namespace nearby {
namespace presence {

// Implements the C++ backed parsing and decrypting of advertisement bytes
class AdvertisementDecoderImpl : public AdvertisementDecoder {
 public:
  AdvertisementDecoderImpl() = default;
  explicit AdvertisementDecoderImpl(
      absl::flat_hash_map<nearby::internal::IdentityType,
                          std::vector<internal::SharedCredential>>*
          credentials_map)
      : credentials_map_(credentials_map) {}

  absl::StatusOr<Advertisement> DecodeAdvertisement(
      absl::string_view advertisement) override;

 private:
  absl::flat_hash_map<internal::IdentityType,
                      std::vector<internal::SharedCredential>>*
      credentials_map_ = nullptr;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_DECODER_IMPL_H_
