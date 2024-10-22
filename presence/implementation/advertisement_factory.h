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

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/mediums/advertisement_data.h"

namespace nearby {
namespace presence {

// Builds BLE advertisements from broadcast requests.
class AdvertisementFactory {
 public:
  using LocalCredential = internal::LocalCredential;

  // Returns a `CredentialSelector` if credentials are required to create an
  // advertisement from the `request`.
  static absl::StatusOr<CredentialSelector> GetCredentialSelector(
      const BaseBroadcastRequest& request);

  // Returns a BLE advertisement for given `request.
  absl::StatusOr<AdvertisementData> CreateAdvertisement(
      const BaseBroadcastRequest& request,
      absl::optional<LocalCredential> credential) const;  // NOLINT

  absl::StatusOr<AdvertisementData> CreateAdvertisement(
      const BaseBroadcastRequest& request) const {
    return CreateAdvertisement(request,
                               absl::optional<LocalCredential>());  // NOLINT
  }

 private:
  absl::StatusOr<AdvertisementData> CreateBaseNpAdvertisement(
      const BaseBroadcastRequest& request,
      absl::optional<LocalCredential> credential) const;  // NOLINT
  absl::StatusOr<std::string> EncryptDataElements(
      const LocalCredential& credential, absl::string_view salt,
      absl::string_view data_elements) const;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_ADVERTISEMENT_FACTORY_H_
