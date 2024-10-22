// Copyright 2020 Google LLC
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

#include "presence/presence_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "internal/platform/borrowable.h"
#include "presence/data_types.h"
#include "presence/presence_client_impl.h"
#include "presence/presence_device_provider.h"

namespace nearby {
namespace presence {

std::unique_ptr<PresenceClient> PresenceServiceImpl::CreatePresenceClient() {
  return PresenceClientImpl::Factory::Create(lender_.GetBorrowable());
}

absl::StatusOr<ScanSessionId> PresenceServiceImpl::StartScan(
    ScanRequest scan_request, ScanCallback callback) {
  return service_controller_.StartScan(scan_request, std::move(callback));
}

void PresenceServiceImpl::StopScan(ScanSessionId id) {
  service_controller_.StopScan(id);
}

absl::StatusOr<BroadcastSessionId> PresenceServiceImpl::StartBroadcast(
    BroadcastRequest broadcast_request, BroadcastCallback callback) {
  return service_controller_.StartBroadcast(broadcast_request,
                                            std::move(callback));
}

void PresenceServiceImpl::StopBroadcast(BroadcastSessionId session) {
  service_controller_.StopBroadcast(session);
}

void PresenceServiceImpl::UpdateDeviceIdentityMetaData(
    const ::nearby::internal::DeviceIdentityMetaData& device_identity_metadata,
    bool regen_credentials, absl::string_view manager_app_id,
    const std::vector<nearby::internal::IdentityType>& identity_types,
    int credential_life_cycle_days, int contiguous_copy_of_credentials,
    GenerateCredentialsResultCallback credentials_generated_cb) {
  provider_.UpdateDeviceIdentityMetaData(device_identity_metadata);
  provider_.SetManagerAppId(manager_app_id);
  service_controller_.UpdateDeviceIdentityMetaData(
      device_identity_metadata, regen_credentials, manager_app_id,
      identity_types, credential_life_cycle_days,
      contiguous_copy_of_credentials, std::move(credentials_generated_cb));
}

void PresenceServiceImpl::GetLocalPublicCredentials(
    const CredentialSelector& credential_selector,
    GetPublicCredentialsResultCallback callback) {
  service_controller_.GetLocalPublicCredentials(credential_selector,
                                                std::move(callback));
}

void PresenceServiceImpl::UpdateRemotePublicCredentials(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<nearby::internal::SharedCredential>& remote_public_creds,
    UpdateRemotePublicCredentialsCallback credentials_updated_cb) {
  service_controller_.UpdateRemotePublicCredentials(
      manager_app_id, account_name, remote_public_creds,
      std::move(credentials_updated_cb));
}

}  // namespace presence
}  // namespace nearby
