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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_DISCOVERY_FILTER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_DISCOVERY_FILTER_H_

#include <vector>

#include "internal/proto/credential.pb.h"
#include "presence/presence_action.h"
#include "presence/presence_zone.h"
namespace nearby {
namespace presence {
class DiscoveryFilter {
 public:
  DiscoveryFilter(const std::vector<PresenceAction>& = {},
                  const std::vector<::nearby::internal::IdentityType>& = {},
                  const std::vector<PresenceZone>& = {}) noexcept;
  std::vector<PresenceAction> GetActions() const;
  std::vector<::nearby::internal::IdentityType> GetIdentities() const;
  std::vector<PresenceZone> GetZones() const;

 private:
  const std::vector<PresenceAction> actions_;
  const std::vector<::nearby::internal::IdentityType> identities_;
  const std::vector<PresenceZone> zones_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_DISCOVERY_FILTER_H_
