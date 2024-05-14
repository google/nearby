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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_BROADCAST_REQUEST_H_
#define THIRD_PARTY_NEARBY_PRESENCE_BROADCAST_REQUEST_H_

#include <string>
#include <vector>

#include "absl/types/variant.h"
#include "internal/proto/credential.pb.h"
#include "presence/data_element.h"
#include "presence/power_mode.h"

namespace nearby {
namespace presence {

// Broadcast parameter for presence features.
struct PresenceBroadcast {
  struct BroadcastSection {
    // Presence identity type.
    ::nearby::internal::IdentityType identity =
        ::nearby::internal::IdentityType::IDENTITY_TYPE_UNSPECIFIED;

    // Additional Data Elements.
    // The Presence SDK generates:
    // - Salt,
    // - (Private/Trusted/Public/Provisioned) Identity,
    // - TX power,
    // - Advertisement signature
    // Data Elements when they are required in the advertisement. Other Data
    // Elements are provided by the client application.
    // Nearby SDK encrypts Data ELements before broadcasting if a non-public
    // `PresenceIdentity` is provided.
    std::vector<DataElement> extended_properties;

    // Account name used to select private credentials.
    std::string account_name;

    // Manager app id, used to select private credentials.
    std::string manager_app_id;
  };

  std::vector<BroadcastSection> sections;
};

// Broadcast request for legacy Android T, which needs to provide credential
// and salt in the broadcast parameters.
// TODO(b/243443813) - Support Legacy Broadcast Request
struct LegacyPresenceBroadcast {};

// Nearby Presence advertisement request options.
struct BroadcastRequest {
  // Calibrated TX power. The broadcast recipient uses it to calculate the
  // distance between both devices.
  int tx_power;

  // The broadcast frequency hint.
  PowerMode power_mode;

  absl::variant<PresenceBroadcast, LegacyPresenceBroadcast> variant;
};

}  // namespace presence
}  // namespace nearby
#endif  // THIRD_PARTY_NEARBY_PRESENCE_BROADCAST_REQUEST_H_
