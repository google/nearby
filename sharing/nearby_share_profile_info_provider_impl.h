// Copyright 2022-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARE_PROFILE_INFO_PROVIDER_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARE_PROFILE_INFO_PROVIDER_IMPL_H_

#include <optional>
#include <string>

#include "internal/platform/device_info.h"
#include "internal/platform/implementation/account_manager.h"
#include "sharing/common/nearby_share_profile_info_provider.h"

namespace nearby {
namespace sharing {

// An implementation of NearbyShareProfileInfoProvider that accesses the actual
// profile data.
class NearbyShareProfileInfoProviderImpl
    : public NearbyShareProfileInfoProvider {
 public:
  NearbyShareProfileInfoProviderImpl(nearby::DeviceInfo& device_info,
                                     AccountManager& account_manager);
  NearbyShareProfileInfoProviderImpl(
      const NearbyShareProfileInfoProviderImpl&) = delete;
  NearbyShareProfileInfoProviderImpl& operator=(
      const NearbyShareProfileInfoProviderImpl&) = delete;
  ~NearbyShareProfileInfoProviderImpl() override;

  // NearbyShareProfileInfoProvider:
  std::optional<std::string> GetGivenName() const override;
  std::optional<std::string> GetProfileUserName() const override;

 private:
  nearby::DeviceInfo& device_info_;
  AccountManager& account_manager_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARE_PROFILE_INFO_PROVIDER_IMPL_H_
