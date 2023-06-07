// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FASTPAIR_SWITCHES_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FASTPAIR_SWITCHES_H_

#include <string>

#include "fastpair/common/fast_pair_switches.h"

namespace nearby {
namespace fastpair {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.

// Setter and Getter for Nearby Fast Pair http host.
void SetNearbyFastPairHttpHost(const std::string& host);
std::string GetNearbyFastPairHttpHost();

}  // namespace switches
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FASTPAIR_SWITCHES_H_
