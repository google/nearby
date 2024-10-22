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

#include "presence/implementation/service_controller_impl.h"

#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "presence/data_types.h"
#include "presence/implementation/credential_manager.h"

namespace nearby {
namespace presence {

absl::StatusOr<ScanSessionId> ServiceControllerImpl::StartScan(
    ScanRequest scan_request, ScanCallback callback) {
  return scan_manager_.StartScan(scan_request, std::move(callback));
}
void ServiceControllerImpl::StopScan(ScanSessionId id) {
  scan_manager_.StopScan(id);
}

absl::StatusOr<BroadcastSessionId> ServiceControllerImpl::StartBroadcast(
    BroadcastRequest broadcast_request, BroadcastCallback callback) {
  return broadcast_manager_.StartBroadcast(broadcast_request,
                                           std::move(callback));
}

void ServiceControllerImpl::StopBroadcast(BroadcastSessionId id) {
  broadcast_manager_.StopBroadcast(id);
}

// TODO(b/327629276): Remove this function.
void ServiceControllerImpl::UpdateLocalDeviceMetadata(
    const ::nearby::internal::Metadata& metadata, bool regen_credentials,
    absl::string_view manager_app_id,
    const std::vector<nearby::internal::IdentityType>& identity_types,
    int credential_life_cycle_days, int contiguous_copy_of_credentials,
    GenerateCredentialsResultCallback credentials_generated_cb) {}

void ServiceControllerImpl::UpdateDeviceIdentityMetaData(
    const ::nearby::internal::DeviceIdentityMetaData& device_identity_metadata,
    bool regen_credentials, absl::string_view manager_app_id,
    const std::vector<nearby::internal::IdentityType>& identity_types,
    int credential_life_cycle_days, int contiguous_copy_of_credentials,
    GenerateCredentialsResultCallback credentials_generated_cb) {
  credential_manager_.SetDeviceIdentityMetaData(
      device_identity_metadata, regen_credentials, manager_app_id,
      identity_types, credential_life_cycle_days,
      contiguous_copy_of_credentials, std::move(credentials_generated_cb));
}

void ServiceControllerImpl::GetLocalPublicCredentials(
    const CredentialSelector& credential_selector,
    GetPublicCredentialsResultCallback callback) {
  credential_manager_.GetPublicCredentials(
      credential_selector, PublicCredentialType::kLocalPublicCredential,
      std::move(callback));
}

void ServiceControllerImpl::UpdateRemotePublicCredentials(
    absl::string_view manager_app_id, absl::string_view account_name,
    const std::vector<nearby::internal::SharedCredential>& remote_public_creds,
    UpdateRemotePublicCredentialsCallback credentials_updated_cb) {
  credential_manager_.UpdateRemotePublicCredentials(
      manager_app_id, account_name, remote_public_creds,
      std::move(credentials_updated_cb));
}

void ServiceControllerImpl::GetLocalCredentials(
    const CredentialSelector& credential_selector,
    GetLocalCredentialsResultCallback callback) {
  credential_manager_.GetLocalCredentials(credential_selector,
                                          std::move(callback));
}

}  // namespace presence
}  // namespace nearby
