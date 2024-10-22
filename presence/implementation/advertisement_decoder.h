// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ADVERTISEMENT_DECODER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ADVERTISEMENT_DECODER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/proto/credential.pb.h"
#include "presence/data_element.h"

namespace nearby {
namespace presence {

// The structured decoded form of a detected Nearby Presence advertisement
struct Advertisement {
  uint8_t version = 0;
  std::vector<DataElement> data_elements;
  absl::StatusOr<internal::SharedCredential> public_credential =
      absl::NotFoundError("");
  internal::IdentityType identity_type = internal::IDENTITY_TYPE_UNSPECIFIED;
  std::string metadata_key;
};

// Interface for decoding Nearby Presence advertisements from a payload of raw
// bytes into a structured, decrypted, and decoded format
class AdvertisementDecoder {
 public:
  // Is needed otherwise deleting an instance via a pointer to a base class
  // results in undefined behavior
  virtual ~AdvertisementDecoder() = default;

  // Returns the structured and decoded contents of an advertisement given a
  // payload of bytes as a string. Returns an error if the advertisement is
  // misformatted or if it couldn't be decrypted.
  virtual absl::StatusOr<Advertisement> DecodeAdvertisement(
      absl::string_view advertisement) = 0;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_ADVERTISEMENT_DECODER_H_
