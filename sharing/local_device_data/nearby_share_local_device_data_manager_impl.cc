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

#include "sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "internal/platform/device_info.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/implementation/device_info.h"
#include "proto/identity/v1/resources.pb.h"
#include "proto/identity/v1/rpcs.pb.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/base/utf_string_conversions.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/device_rpc.pb.h"
#include "sharing/proto/field_mask.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/timestamp.pb.h"

namespace nearby {
namespace sharing {
namespace {
using ::nearby::api::DeviceInfo;
using ::nearby::sharing::api::PreferenceManager;

constexpr absl::string_view kDefaultDeviceName = "$0\'s $1";

// Returns a truncated version of |name| that is |max_length| characters long.
// For example, name="Reallylongname" with max_length=9 will return "Really...".
// name="Reallylongname" with max_length=20 will return "Reallylongname".
std::string GetTruncatedName(std::string name, size_t max_length) {
  if (name.length() <= max_length) {
    return name;
  }

  std::string ellipsis("...");
  size_t max_name_length = max_length - ellipsis.length();

  std::string truncated;
  nearby::utils::TruncateUtf8ToByteSize(name, max_name_length, &truncated);
  truncated.append(ellipsis);
  return truncated;
}

}  // namespace

// static
NearbyShareLocalDeviceDataManagerImpl::Factory*
    NearbyShareLocalDeviceDataManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareLocalDeviceDataManager>
NearbyShareLocalDeviceDataManagerImpl::Factory::Create(
    PreferenceManager& preference_manager,
    AccountManager& account_manager, nearby::DeviceInfo& device_info) {
  if (test_factory_) {
    return test_factory_->CreateInstance();
  }

  return absl::WrapUnique(new NearbyShareLocalDeviceDataManagerImpl(
      preference_manager, account_manager, device_info));
}

// static
void NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareLocalDeviceDataManagerImpl::Factory::~Factory() = default;

NearbyShareLocalDeviceDataManagerImpl::NearbyShareLocalDeviceDataManagerImpl(
    PreferenceManager& preference_manager, AccountManager& account_manager,
    nearby::DeviceInfo& device_info)
    : preference_manager_(preference_manager),
      account_manager_(account_manager),
      device_info_(device_info) {}

NearbyShareLocalDeviceDataManagerImpl::
    ~NearbyShareLocalDeviceDataManagerImpl() = default;

std::string NearbyShareLocalDeviceDataManagerImpl::GetDeviceName() const {
  std::string device_name = preference_manager_.GetString(
      prefs::kNearbySharingDeviceNameName, std::string());
  return device_name.empty() ? GetDefaultDeviceName() : device_name;
}

DeviceNameValidationResult
NearbyShareLocalDeviceDataManagerImpl::ValidateDeviceName(
    absl::string_view name) {
  if (name.empty()) return DeviceNameValidationResult::kErrorEmpty;

  if (!nearby::utils::IsStringUtf8(std::string_view(name.data(), name.size())))
    return DeviceNameValidationResult::kErrorNotValidUtf8;

  if (name.length() > kNearbyShareDeviceNameMaxLength)
    return DeviceNameValidationResult::kErrorTooLong;

  return DeviceNameValidationResult::kValid;
}

DeviceNameValidationResult NearbyShareLocalDeviceDataManagerImpl::SetDeviceName(
    absl::string_view name) {
  if (name == GetDeviceName()) return DeviceNameValidationResult::kValid;

  auto error = ValidateDeviceName(name);
  if (error != DeviceNameValidationResult::kValid) return error;

  preference_manager_.SetString(prefs::kNearbySharingDeviceNameName, name);

  NotifyLocalDeviceDataChanged(/*did_device_name_change=*/true,
                               /*did_full_name_change=*/false,
                               /*did_icon_change=*/false);

  return DeviceNameValidationResult::kValid;
}

std::string NearbyShareLocalDeviceDataManagerImpl::GetDefaultDeviceName()
    const {
  std::optional<AccountManager::Account> account =
      account_manager_.GetCurrentAccount();
  DeviceInfo::OsType os_type = device_info_.GetOsType();

  // If not logged in or account has not given name, use machine name instead.
  // For iOS, macOS and visionOS, the device name is already localized and
  // generally works well for Quick Share purposes (i.e. "Niko's MacBook Pro"),
  // so avoid using the non-localized account name and device type
  // concatenation.
  if (os_type == DeviceInfo::OsType::kMacOS ||
      os_type == DeviceInfo::OsType::kIos ||
      os_type == DeviceInfo::OsType::kVisionOS ||
      !account.has_value() ||
      account->given_name.empty()) {
    std::string device_name = device_info_.GetOsDeviceName();
    return GetTruncatedName(device_name, kNearbyShareDeviceNameMaxLength);
  }
  std::string given_name = account->given_name;
  std::string device_type = device_info_.GetDeviceTypeName();
  uint64_t untruncated_length =
      absl::Substitute(kDefaultDeviceName, given_name, device_type).length();
  uint64_t overflow_length =
      untruncated_length - kNearbyShareDeviceNameMaxLength;

  std::string truncated_name =
      GetTruncatedName(given_name, given_name.length() - overflow_length);

  return absl::Substitute(kDefaultDeviceName, truncated_name, device_type);
}

}  // namespace sharing
}  // namespace nearby
