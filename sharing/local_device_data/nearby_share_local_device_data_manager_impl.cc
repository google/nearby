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

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "internal/platform/device_info.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/implementation/device_info.h"
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
#include "sharing/scheduling/nearby_share_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler_factory.h"

namespace nearby {
namespace sharing {
namespace {
using ::nearby::api::DeviceInfo;
using ::nearby::sharing::api::PreferenceManager;
using ::nearby::sharing::api::SharingRpcClientFactory;
using ::nearby::sharing::proto::UpdateDeviceRequest;
using ::nearby::sharing::proto::UpdateDeviceResponse;

// Using the alphanumeric characters below, this provides 36^10 unique device
// IDs. Note that the uniqueness requirement is not global; the IDs are only
// used to differentiate between devices associated with a single GAIA account.
// This ID length agrees with the GmsCore implementation.
constexpr size_t kDeviceIdLength = 10;

// Possible characters used in a randomly generated device ID. This agrees with
// the GmsCore implementation.
constexpr std::array<char, 36> kAlphaNumericChars = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

constexpr absl::string_view kDeviceIdPrefix = "users/me/devices/";
constexpr absl::string_view kContactsFieldMaskPath = "contacts";
constexpr absl::string_view kCertificatesFieldMaskPath = "public_certificates";

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
      nearby_share_client_(rpc_client_factory->CreateInstance()),
      device_id_(GetId()),
      executor_(context->CreateSequencedTaskRunner()) {}

NearbyShareLocalDeviceDataManagerImpl::
    ~NearbyShareLocalDeviceDataManagerImpl() = default;

std::string NearbyShareLocalDeviceDataManagerImpl::GetId() {
  std::string id =
      preference_manager_.GetString(prefs::kNearbySharingDeviceIdName, "");
  if (!id.empty()) return id;

  absl::BitGen bitgen;
  for (size_t i = 0; i < kDeviceIdLength; ++i)
    id += kAlphaNumericChars[absl::Uniform(
        bitgen, 0, static_cast<int>(kAlphaNumericChars.size()))];

  preference_manager_.SetString(prefs::kNearbySharingDeviceIdName, id);

  return id;
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

void NearbyShareLocalDeviceDataManagerImpl::UploadContacts(
    std::vector<nearby::sharing::proto::Contact> contacts,
    UploadCompleteCallback callback) {
  executor_->PostTask(
      [&, contacts = std::move(contacts), callback = std::move(callback)]() {
        LOG(INFO) << __func__ << ": size=" << contacts.size();
        if (!is_running()) {
          LOG(WARNING) << "UploadContacts: skip to upload contacts due "
                          "to manager is stopped.";
          callback(false);
          return;
        }

        if (!account_manager_.GetCurrentAccount().has_value()) {
          LOG(WARNING) << __func__
                       << ": skip to upload contacts due "
                          "to no login account.";
          callback(/*success=*/true);
          return;
        }

        UpdateDeviceRequest request;
        request.mutable_device()->set_name(
            absl::StrCat(kDeviceIdPrefix, device_id_));
        request.mutable_device()->mutable_contacts()->Add(contacts.begin(),
                                                          contacts.end());
        request.mutable_update_mask()->add_paths(
            std::string(kContactsFieldMaskPath));
        nearby_share_client_->UpdateDevice(
            request, [callback = std::move(callback)](
                         const absl::StatusOr<UpdateDeviceResponse>& response) {
              if (!response.ok()) {
                LOG(WARNING)
                    << "UploadContacts: Failed to get response from backend: "
                    << response.status();
              }
              callback(/*success=*/response.ok());
            });
      });
}

void NearbyShareLocalDeviceDataManagerImpl::UploadCertificates(
    std::vector<nearby::sharing::proto::PublicCertificate> certificates,
    UploadCompleteCallback callback) {
  executor_->PostTask([&, certificates = std::move(certificates),
                       callback = std::move(callback)]() {
    LOG(INFO) << __func__ << ": Upload " << certificates.size()
              << " certificates.";
    if (!is_running()) {
      LOG(WARNING) << "UploadContacts: skip to upload certificates due "
                      "to manager is stopped.";
      callback(false);
      return;
    }

    if (!account_manager_.GetCurrentAccount().has_value()) {
      LOG(WARNING) << __func__
                   << ": skip to upload certificates due "
                      "to no login account.";
      callback(/*success=*/true);
      return;
    }

    UpdateDeviceRequest request;
    request.mutable_device()->set_name(
        absl::StrCat(kDeviceIdPrefix, device_id_));
    request.mutable_device()->mutable_public_certificates()->Add(
        certificates.begin(), certificates.end());
    request.mutable_update_mask()->add_paths(
        std::string(kCertificatesFieldMaskPath));
    nearby_share_client_->UpdateDevice(
        request, [this, callback = std::move(callback)](
                     const absl::StatusOr<UpdateDeviceResponse>& response) {
          // check whether the manager is running again
          if (!is_running()) {
            LOG(WARNING)
                << "DownloadDeviceData: skip to upload certificates due "
                   "to manager is stopped.";
            callback(false);
            return;
          }
          if (!response.ok()) {
            LOG(WARNING)
                << "UploadCertificates: Failed to get response from backend: "
                << response.status();
          }
          callback(/*success=*/response.ok());
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
