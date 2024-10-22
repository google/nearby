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
#include "sharing/common/nearby_share_enums.h"
#include "sharing/common/nearby_share_prefs.h"
#include "sharing/common/nearby_share_profile_info_provider.h"
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

constexpr absl::Duration kDeviceDataDownloadPeriod = absl::Hours(12);

constexpr absl::string_view kDefaultDeviceName = "$0\'s $1";

// Returns a truncated version of |name| that is |overflow_length| characters
// too long. For example, name="Reallylongname" with overflow_length=5 will
// return "Really...".
std::string GetTruncatedName(std::string name, size_t overflow_length) {
  std::string ellipsis("...");
  size_t max_name_length = name.length() - overflow_length - ellipsis.length();
  // DCHECK_GT(max_name_length, 0u);

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
    SharingRpcClientFactory* rpc_client_factory,
    NearbyShareProfileInfoProvider* profile_info_provider) {
  if (test_factory_) {
    return test_factory_->CreateInstance(context, rpc_client_factory,
                                         profile_info_provider);
  }

  return absl::WrapUnique(new NearbyShareLocalDeviceDataManagerImpl(
      context, preference_manager, account_manager, device_info,
      rpc_client_factory, profile_info_provider));
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
    SharingRpcClientFactory* rpc_client_factory,
    NearbyShareProfileInfoProvider* profile_info_provider)
    : preference_manager_(preference_manager),
      account_manager_(account_manager),
      device_info_(device_info),
      profile_info_provider_(profile_info_provider),
      nearby_share_client_(rpc_client_factory->CreateInstance()),
      device_id_(GetId()),
      download_device_data_scheduler_(
          NearbyShareSchedulerFactory::CreatePeriodicScheduler(
              context, preference_manager_, kDeviceDataDownloadPeriod,
              /*retry_failures=*/true,
              /*require_connectivity=*/true,
              prefs::kNearbySharingSchedulerDownloadDeviceDataName,
              [&]() { DownloadDeviceData(); })),
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

std::optional<std::string> NearbyShareLocalDeviceDataManagerImpl::GetFullName()
    const {
  return preference_manager_.GetString(prefs::kNearbySharingFullNameName,
                                       std::string());
}

std::optional<std::string> NearbyShareLocalDeviceDataManagerImpl::GetIconUrl()
    const {
  return preference_manager_.GetString(prefs::kNearbySharingIconUrlName,
                                       std::string());
}

std::optional<std::string> NearbyShareLocalDeviceDataManagerImpl::GetIconToken()
    const {
  return preference_manager_.GetString(prefs::kNearbySharingIconTokenName,
                                       std::string());
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

void NearbyShareLocalDeviceDataManagerImpl::DownloadDeviceData() {
  executor_->PostTask([&]() {
    NL_LOG(INFO) << __func__ << ": started";
    if (!is_running()) {
      NL_LOG(WARNING) << "DownloadDeviceData: skip to download device data due "
                         "to manager is stopped.";
      return;
    }

    if (!account_manager_.GetCurrentAccount().has_value()) {
      NL_LOG(WARNING) << __func__
                      << ": skip to download device data due "
                         "to no login account.";
      download_device_data_scheduler_->HandleResult(/*success=*/true);
      return;
    }

    UpdateDeviceRequest request;
    request.mutable_device()->set_name(
        absl::StrCat(kDeviceIdPrefix, device_id_));
    nearby_share_client_->UpdateDevice(
        request, [this](const absl::StatusOr<UpdateDeviceResponse>& response) {
          // check whether the manager is running again
          if (!is_running()) {
            NL_LOG(WARNING)
                << "DownloadDeviceData: skip to download device data due "
                   "to manager is stopped.";
            return;
          }

          if (response.ok()) {
            NL_LOG(WARNING) << "DownloadDeviceData: Got response from backend.";
            HandleUpdateDeviceResponse(*response);
          } else {
            NL_LOG(WARNING)
                << "DownloadDeviceData: Failed to get response from backend: "
                << response.status();
          }

          download_device_data_scheduler_->HandleResult(
              /*success=*/response.ok());
        });
  });
}

void NearbyShareLocalDeviceDataManagerImpl::UploadContacts(
    std::vector<nearby::sharing::proto::Contact> contacts,
    UploadCompleteCallback callback) {
  executor_->PostTask(
      [&, contacts = std::move(contacts), callback = std::move(callback)]() {
        NL_LOG(INFO) << __func__ << ": size=" << contacts.size();
        if (!is_running()) {
          NL_LOG(WARNING) << "UploadContacts: skip to upload contacts due "
                             "to manager is stopped.";
          callback(false);
          return;
        }

        if (!account_manager_.GetCurrentAccount().has_value()) {
          NL_LOG(WARNING) << __func__
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
                NL_LOG(WARNING)
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
    NL_LOG(INFO) << __func__ << ": Upload " << certificates.size()
                 << " certificates.";
    if (!is_running()) {
      NL_LOG(WARNING) << "UploadContacts: skip to upload certificates due "
                         "to manager is stopped.";
      callback(false);
      return;
    }

    if (!account_manager_.GetCurrentAccount().has_value()) {
      NL_LOG(WARNING) << __func__
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
            NL_LOG(WARNING)
                << "DownloadDeviceData: skip to upload certificates due "
                   "to manager is stopped.";
            callback(false);
            return;
          }
          if (!response.ok()) {
            NL_LOG(WARNING)
                << "UploadCertificates: Failed to get response from backend: "
                << response.status();
          }
          callback(/*success=*/response.ok());
        });
  });
}

void NearbyShareLocalDeviceDataManagerImpl::OnStart() {
  // This schedules an immediate download of the full name and icon URL from the
  // server if that has never happened before.
  download_device_data_scheduler_->Start();
}

void NearbyShareLocalDeviceDataManagerImpl::OnStop() {
  download_device_data_scheduler_->Stop();
}

std::string NearbyShareLocalDeviceDataManagerImpl::GetDefaultDeviceName()
    const {
  std::string device_type_name = device_info_.GetDeviceTypeName();
  std::string device_name = device_info_.GetOsDeviceName();
  std::optional<std::string> given_name =
      profile_info_provider_->GetGivenName();
  if (!given_name.has_value()) return device_name;

  std::string default_device_name =
      absl::Substitute(kDefaultDeviceName, *given_name, device_type_name);

  if (default_device_name.length() <= kNearbyShareDeviceNameMaxLength)
    return default_device_name;

  std::string truncated_name =
      GetTruncatedName(*given_name, default_device_name.length() -
                                        kNearbyShareDeviceNameMaxLength);

  return absl::Substitute(kDefaultDeviceName, truncated_name, device_type_name);
}

void NearbyShareLocalDeviceDataManagerImpl::HandleUpdateDeviceResponse(
    const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
        response) {
  if (!response) return;

  bool did_full_name_change = response->person_name() != GetFullName();
  if (did_full_name_change) {
    preference_manager_.SetString(prefs::kNearbySharingFullNameName,
                                  response->person_name());
  }

  // NOTE(http://crbug.com/1211189): An icon URL can change without the
  // underlying image changing. For example, icon URLs for some child accounts
  // can rotate on every UpdateDevice RPC call; a timestamp is included in the
  // URL. The icon token is used to detect changes in the underlying image. If a
  // new URL is sent and the token doesn't change, the old URL may still be
  // valid for a couple of weeks, for example. So, private certificates do not
  // necessarily need to update the icon URL whenever it changes. Also, we don't
  // expect the token to change without the URL changing; regardless, we don't
  // consider the icon changed unless the URL changes. That way, private
  // certificates will not be unnecessarily regenerated.
  bool did_icon_url_change = response->image_url() != GetIconUrl();
  bool did_icon_token_change = response->image_token() != GetIconToken();
  bool did_icon_change = did_icon_url_change && did_icon_token_change;
  if (did_icon_url_change) {
    preference_manager_.SetString(prefs::kNearbySharingIconUrlName,
                                  response->image_url());
  }
  if (did_icon_token_change) {
    preference_manager_.SetString(prefs::kNearbySharingIconTokenName,
                                  response->image_token());
  }

  if (!did_full_name_change && !did_icon_change) return;

  NotifyLocalDeviceDataChanged(/*did_device_name_change=*/false,
                               did_full_name_change, did_icon_change);
}

}  // namespace sharing
}  // namespace nearby
