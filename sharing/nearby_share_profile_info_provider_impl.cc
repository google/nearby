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

#include "sharing/nearby_share_profile_info_provider_impl.h"

#include <optional>
#include <string>

#include "internal/platform/device_info.h"
#include "internal/platform/implementation/account_manager.h"

namespace nearby {
namespace sharing {

NearbyShareProfileInfoProviderImpl::NearbyShareProfileInfoProviderImpl(
    nearby::DeviceInfo& device_info, AccountManager& account_manager)
    : device_info_(device_info), account_manager_(account_manager) {}

NearbyShareProfileInfoProviderImpl::~NearbyShareProfileInfoProviderImpl() =
    default;

std::optional<std::string> NearbyShareProfileInfoProviderImpl::GetGivenName()
    const {
  // Use the given name when the user logs in to the backend.
  std::optional<AccountManager::Account> account =
      account_manager_.GetCurrentAccount();

  if (account.has_value() && !account->given_name.empty()) {
    return account->given_name;
  }

  return device_info_.GetGivenName();
}

std::optional<std::string>
NearbyShareProfileInfoProviderImpl::GetProfileUserName() const {
  return device_info_.GetProfileUserName();
}

}  // namespace sharing
}  // namespace nearby
