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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_H_

#include <vector>

#include "absl/status/statusor.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/proto/metadata.pb.h"
#include "presence/broadcast_request.h"
#include "presence/data_types.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

/*
 * This class is owned in {@code PresenceService}. It specifies the function
 * signatures. {@code ServiceControllerImpl} and {@code MockServiceController}
 * inherit this class and provides real implementation and mock impl for tests.
 */
class ServiceController {
 public:
  ServiceController() = default;
  virtual ~ServiceController() = default;
  virtual absl::StatusOr<ScanSessionId> StartScan(ScanRequest scan_request,
                                                  ScanCallback callback) = 0;
  virtual void StopScan(ScanSessionId session_id) = 0;
  virtual absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback) = 0;
  virtual void StopBroadcast(BroadcastSessionId session_id) = 0;
  virtual void UpdateLocalDeviceMetadata(
      const ::nearby::internal::Metadata& metadata, bool regen_credentials,
      absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsResultCallback credentials_generated_cb) = 0;
  virtual void UpdateDeviceIdentityMetaData(
      const ::nearby::internal::DeviceIdentityMetaData&
          device_identity_metadata,
      bool regen_credentials, absl::string_view manager_app_id,
      const std::vector<nearby::internal::IdentityType>& identity_types,
      int credential_life_cycle_days, int contiguous_copy_of_credentials,
      GenerateCredentialsResultCallback credentials_generated_cb) = 0;
  virtual ::nearby::internal::DeviceIdentityMetaData
  GetDeviceIdentityMetaData() = 0;
  virtual void GetLocalPublicCredentials(
      const CredentialSelector& credential_selector,
      GetPublicCredentialsResultCallback callback) = 0;
  virtual void UpdateRemotePublicCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<nearby::internal::SharedCredential>&
          remote_public_creds,
      UpdateRemotePublicCredentialsCallback credentials_updated_cb) = 0;
  virtual void GetLocalCredentials(
      const CredentialSelector& credential_selector,
      GetLocalCredentialsResultCallback callback) = 0;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_SERVICE_CONTROLLER_H_
