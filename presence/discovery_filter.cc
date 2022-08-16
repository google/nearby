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

#include "presence/discovery_filter.h"

namespace nearby {
namespace presence {

using ::nearby::internal::IdentityType;

DiscoveryFilter::DiscoveryFilter(
    const std::vector<PresenceAction>& actions,
    const std::vector<IdentityType>& identities,
    const std::vector<PresenceZone>& zones) noexcept
    : actions_(actions), identities_(identities), zones_(zones) {}
std::vector<PresenceAction> DiscoveryFilter::GetActions() const {
  return actions_;
}
std::vector<IdentityType> DiscoveryFilter::GetIdentities() const {
  return identities_;
}
std::vector<PresenceZone> DiscoveryFilter::GetZones() const { return zones_; }

}  // namespace presence
}  // namespace nearby
