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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_CERTIFICATE_MANAGER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_CERTIFICATE_MANAGER_H_
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "third_party/nearby/presence/presence_identity.h"

namespace nearby {
namespace presence {

/** Stores and manages credentials/certificates on local device, and uses the
 * certificates for encryption and deceryption. */
class CertificateManager {
 public:
  virtual ~CertificateManager() = default;

  /** Returns encrypted metadata key associated with `identity` for Base NP
   * advertisement */
  virtual absl::StatusOr<std::string> GetBaseEncryptedMetadataKey(
      const PresenceIdentity& identity) = 0;
  /** Encrypts `data_elements` using certificate associated with `identity` and
   * `salt` */
  virtual absl::StatusOr<std::string> EncryptDataElements(
      const PresenceIdentity& identity, absl::string_view salt,
      absl::string_view data_elements) = 0;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_CERTIFICATE_MANAGER_H_
