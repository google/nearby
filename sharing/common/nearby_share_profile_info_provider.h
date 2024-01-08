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

#ifndef THIRD_PARTY_NEARBY_SHARING_COMMON_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_
#define THIRD_PARTY_NEARBY_SHARING_COMMON_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_

#include <optional>
#include <string>

namespace nearby {
namespace sharing {
class NearbyShareProfileInfoProvider {
 public:
  NearbyShareProfileInfoProvider() = default;
  virtual ~NearbyShareProfileInfoProvider() = default;

  // Returns UTF-8 encoded given name of current account.
  // Returns absl::nullopt if a valid given name cannot be returned.
  virtual std::optional<std::string> GetGivenName() const = 0;

  // Proxy for Profile::GetProfileUserName(). Returns absl::nullopt if a valid
  // username cannot be returned.
  virtual std::optional<std::string> GetProfileUserName() const = 0;
};
}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_COMMON_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_
