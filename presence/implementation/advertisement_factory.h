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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_FACTORY_H_
#define THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_FACTORY_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "internal/proto/credential.pb.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/mediums/advertisement_data.h"

namespace nearby {
namespace presence {

// Builds BLE advertisements from broadcast requests.
class AdvertisementFactory {
 public:
  using PrivateCredential = internal::PrivateCredential;

  // Returns a `CredentialSelector` if credentials are required to create an
  // advertisement from the `request`.
  static absl::StatusOr<CredentialSelector> GetCredentialSelector(
      const BaseBroadcastRequest& request);

  // Returns a BLE advertisement for given `request.
  absl::StatusOr<AdvertisementData> CreateAdvertisement(
      const BaseBroadcastRequest& request,
      std::vector<PrivateCredential>& credentials) const;

  absl::StatusOr<AdvertisementData> CreateAdvertisement(
      const BaseBroadcastRequest& request) const {
    std::vector<PrivateCredential> empty;
    return CreateAdvertisement(request, empty);
  }

 private:
  absl::StatusOr<AdvertisementData> CreateBaseNpAdvertisement(
      const BaseBroadcastRequest& request,
      std::vector<PrivateCredential>& credentials) const;
  absl::StatusOr<std::string> EncryptDataElements(
      std::vector<PrivateCredential>& credentials, absl::string_view salt,
      absl::string_view data_elements) const;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_FACTORY_H_
