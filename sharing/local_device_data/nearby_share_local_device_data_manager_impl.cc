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
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/algorithm.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
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
#include "sharing/internal/api/sharing_rpc_client.h"
#include "sharing/internal/base/utf_string_conversions.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/proto/device_rpc.pb.h"
#include "sharing/proto/field_mask.pb.h"
#include "sharing/proto/rpc_resources.pb.h"
#include "sharing/proto/timestamp.pb.h"
#include "util/hash/highway_fingerprint.h"

namespace nearby {
namespace sharing {
namespace {
using ::google::nearby::identity::v1::PublishDeviceRequest;
using ::google::nearby::identity::v1::PublishDeviceResponse;
using ::nearby::api::DeviceInfo;
using ::nearby::sharing::api::PreferenceManager;
using ::nearby::sharing::api::SharingRpcClientFactory;

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
    Context* context, PreferenceManager& preference_manager,
    AccountManager& account_manager, nearby::DeviceInfo& device_info,
    SharingRpcClientFactory* rpc_client_factory) {
  if (test_factory_) {
    return test_factory_->CreateInstance(context, rpc_client_factory);
  }

  return absl::WrapUnique(new NearbyShareLocalDeviceDataManagerImpl(
      context, preference_manager, account_manager, device_info,
      rpc_client_factory));
}

// static
void NearbyShareLocalDeviceDataManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareLocalDeviceDataManagerImpl::Factory::~Factory() = default;

NearbyShareLocalDeviceDataManagerImpl::NearbyShareLocalDeviceDataManagerImpl(
    Context* context, PreferenceManager& preference_manager,
    AccountManager& account_manager, nearby::DeviceInfo& device_info,
    SharingRpcClientFactory* rpc_client_factory)
    : preference_manager_(preference_manager),
      account_manager_(account_manager),
      device_info_(device_info),
      nearby_identity_client_(rpc_client_factory->CreateIdentityInstance()),
      executor_(context->CreateSequencedTaskRunner()) {}

NearbyShareLocalDeviceDataManagerImpl::
    ~NearbyShareLocalDeviceDataManagerImpl() = default;

std::string NearbyShareLocalDeviceDataManagerImpl::GetId() {
  return preference_manager_.GetString(prefs::kNearbySharingDeviceIdName, "");
}

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

void NearbyShareLocalDeviceDataManagerImpl::PublishDevice(
    std::vector<nearby::sharing::proto::PublicCertificate> certificates,
    bool force_update_contacts, PublishDeviceCallback callback) {
  executor_->PostTask([this, force_update_contacts,
                       certificates = std::move(certificates),
                       callback = std::move(callback)]() mutable {
    if (!is_running()) {
      LOG(WARNING) << __func__
                   << ": [Call Identity API] no-op as manager is stopped.";
      callback(/*success=*/false, /*contact_removed=*/false);
      return;
    }
    std::string device_id = GetId();
    if (device_id.empty()) {
      LOG(WARNING) << __func__
                   << ": [Call Identity API] failed, device id is empty.";
      callback(/*success=*/false, /*contact_removed=*/false);
      return;
    }
    LOG(INFO) << __func__ << ": [Call Identity API]  Upload "
              << certificates.size() << " certificates.";

    PublishDeviceRequest request;
    request.mutable_device()->set_name(absl::StrCat("devices/", device_id));
    LOG(INFO) << __func__
              << ": [Call Identity API] PublishDeviceRequest with Device.name: "
              << request.device().name();
    request.mutable_device()->set_display_name(GetDeviceName());
    // Force update contacts call is right after CONTACT_GOOGLE_CONTACT_LATEST
    // call and can use CONTACT_GOOGLE_CONTACT to save server side computation.

    request.mutable_device()->set_contact(
        force_update_contacts
            ? google::nearby::identity::v1::Device::CONTACT_GOOGLE_CONTACT
            : google::nearby::identity::v1::Device::
                  CONTACT_GOOGLE_CONTACT_LATEST);

    auto* new_self_credential =
        request.mutable_device()->add_per_visibility_shared_credentials();
    new_self_credential->set_visibility(
        google::nearby::identity::v1::PerVisibilitySharedCredentials::
            VISIBILITY_SELF);
    auto* new_contacts_credential =
        request.mutable_device()->add_per_visibility_shared_credentials();
    new_contacts_credential->set_visibility(
        google::nearby::identity::v1::PerVisibilitySharedCredentials::
            VISIBILITY_CONTACTS);

    for (const auto& certificate : certificates) {
      // Skip the certificate for deprecated SELECTED_CONTACTS.
      if (certificate.for_selected_contacts()) {
        continue;
      }
      google::nearby::identity::v1::SharedCredential* shared_credential;
      if (certificate.for_self_share()) {
        shared_credential = new_self_credential->add_shared_credentials();
        LOG(INFO) << __func__ << ": [Call Identity API] self_share";
      } else {
        shared_credential = new_contacts_credential->add_shared_credentials();
        LOG(INFO) << __func__ << ": [Call Identity API] contacts_share";
      }

      shared_credential->set_id(
          util_hash::HighwayFingerprint64(certificate.secret_id()));
      shared_credential->set_data(certificate.SerializeAsString());
      shared_credential->set_data_type(
          ::google::nearby::identity::v1::SharedCredential::
              DATA_TYPE_PUBLIC_CERTIFICATE);
      *shared_credential->mutable_expiration_time() = certificate.end_time();
      LOG(INFO) << __func__
                << ": shared_credential.id(): " << shared_credential->id();
    }
    nearby_identity_client_->PublishDevice(
        request, [this, callback = std::move(callback)](
                     const absl::StatusOr<PublishDeviceResponse>& response) {
          // check whether the manager is running again
          if (!is_running()) {
            LOG(WARNING)
                << __func__
                << ": [Call Identity API] manager is stopped after call.";
            callback(/*success=*/false, /*contact_removed=*/false);
            return;
          }
          if (!response.ok()) {
            LOG(WARNING)
                << __func__
                << ": [Call Identity API] Failed to get response from backend: "
                << response.status();
            callback(/*success=*/false, /*contact_removed=*/false);
            return;
          }

          LOG(INFO) << __func__
                    << ": [Call Identity API] Successfully published device.";

          // If contacts are removed, regenerate all Private certificates and
          // make a 2nd PublishDevice RPC call.
          bool need_another_call = false;
          if (absl::linear_search(
                  response.value().contact_updates().begin(),
                  response.value().contact_updates().end(),
                  google::nearby::identity::v1::PublishDeviceResponse::
                      CONTACT_UPDATE_REMOVED)) {
            need_another_call = true;
            LOG(INFO)
                << __func__
                << ": [Call Identity API] need another PublishDevice call";
          }
          callback(/*success=*/true, /*contact_removed=*/need_another_call);
        });
  });
}

std::string NearbyShareLocalDeviceDataManagerImpl::GetDefaultDeviceName()
    const {
  std::optional<AccountManager::Account> account =
      account_manager_.GetCurrentAccount();
  DeviceInfo::OsType os_type = device_info_.GetOsType();

  // If not logged in or account has not given name, use machine name instead.
  // For iOS and macOS, the device name is already localized and generally works
  // well for Quick Share purposes (i.e. "Niko's MacBook Pro"), so avoid using
  // the non-localized account name and device type concatenation.
  if (os_type == DeviceInfo::OsType::kMacOS ||
      os_type == DeviceInfo::OsType::kIos || !account.has_value() ||
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
