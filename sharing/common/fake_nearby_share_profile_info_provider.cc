// Copyright 2021 Google LLC
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

#include "sharing/common/fake_nearby_share_profile_info_provider.h"

#include <optional>
#include <string>

namespace nearby {
namespace sharing {

FakeNearbyShareProfileInfoProvider::FakeNearbyShareProfileInfoProvider() =
    default;

FakeNearbyShareProfileInfoProvider::~FakeNearbyShareProfileInfoProvider() =
    default;

std::optional<std::string> FakeNearbyShareProfileInfoProvider::GetGivenName()
    const {
  return given_name_;
}

std::optional<std::string>
FakeNearbyShareProfileInfoProvider::GetProfileUserName() const {
  return profile_user_name_;
}

}  // namespace sharing
}  // namespace nearby
