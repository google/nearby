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

#ifndef THIRD_PARTY_NEARBY_SHARING_COMMON_FAKE_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_
#define THIRD_PARTY_NEARBY_SHARING_COMMON_FAKE_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_

#include <optional>
#include <string>

#include "sharing/common/nearby_share_profile_info_provider.h"

namespace nearby {
namespace sharing {

class FakeNearbyShareProfileInfoProvider
    : public NearbyShareProfileInfoProvider {
 public:
  FakeNearbyShareProfileInfoProvider();
  ~FakeNearbyShareProfileInfoProvider() override;

  // NearbyShareProfileInfoProvider:
  std::optional<std::string> GetGivenName() const override;
  std::optional<std::string> GetProfileUserName() const override;

  void set_given_name(const std::optional<std::string>& given_name) {
    given_name_ = given_name;
  }

 private:
  std::optional<std::string> given_name_;
  std::optional<std::string> profile_user_name_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_COMMON_FAKE_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_
