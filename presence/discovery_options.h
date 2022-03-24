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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_DISCOVERY_OPTIONS_H_
#define THIRD_PARTY_NEARBY_PRESENCE_DISCOVERY_OPTIONS_H_

namespace nearby {
namespace presence {
struct DiscoveryOptions {
  const bool local_wifi_only_;
};

inline bool operator==(const DiscoveryOptions& o1, const DiscoveryOptions& o2) {
  return o1.local_wifi_only_ == o2.local_wifi_only_;
}

inline bool operator!=(const DiscoveryOptions& o1, const DiscoveryOptions& o2) {
  return !(o1 == o2);
}

}  // namespace presence
}  // namespace nearby
#endif  // THIRD_PARTY_NEARBY_PRESENCE_DISCOVERY_OPTIONS_H_
