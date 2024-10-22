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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_SERVICE_CONTROLLER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_SERVICE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "presence/implementation/service_controller.h"

namespace nearby {
namespace presence {

/*
 * This class is for unit test, mocking {@code ServiceController} functions.
 */
class MockServiceController : public ServiceController {
 public:
  MockServiceController() = default;
  ~MockServiceController() override = default;

  MOCK_METHOD(absl::StatusOr<ScanSessionId>, StartScan,
              (ScanRequest scan_request, ScanCallback callback), (override));
  MOCK_METHOD(void, StopScan, (ScanSessionId session_id), (override));
  MOCK_METHOD(absl::StatusOr<BroadcastSessionId>, StartBroadcast,
              (BroadcastRequest broadcast_request, BroadcastCallback callback),
              (override));
  MOCK_METHOD(void, StopBroadcast, (BroadcastSessionId session_id), (override));
  MOCK_METHOD(
      void, UpdateLocalDeviceMetadata,
      (const ::nearby::internal::Metadata& metadata, bool regen_credentials,
       absl::string_view manager_app_id,
       const std::vector<nearby::internal::IdentityType>& identity_types,
       int credential_life_cycle_days, int contiguous_copy_of_credentials,
       GenerateCredentialsResultCallback credentials_generated_cb),
      (override));
  MOCK_METHOD(
      void, UpdateDeviceIdentityMetaData,
      (const ::nearby::internal::DeviceIdentityMetaData&
           device_identity_metadata,
       bool regen_credentials, absl::string_view manager_app_id,
       const std::vector<nearby::internal::IdentityType>& identity_types,
       int credential_life_cycle_days, int contiguous_copy_of_credentials,
       GenerateCredentialsResultCallback credentials_generated_cb),
      (override));
  MOCK_METHOD(::nearby::internal::DeviceIdentityMetaData,
              GetDeviceIdentityMetaData, (), (override));
  MOCK_METHOD(void, GetLocalPublicCredentials,
              (const CredentialSelector& credential_selector,
               GetPublicCredentialsResultCallback callback),
              (override));
  MOCK_METHOD(void, UpdateRemotePublicCredentials,
              (absl::string_view manager_app_id, absl::string_view account_name,
               const std::vector<nearby::internal::SharedCredential>&
                   remote_public_creds,
               UpdateRemotePublicCredentialsCallback credentials_updated_cb),
              (override));
  MOCK_METHOD(void, GetLocalCredentials,
              (const CredentialSelector& credential_selector,
               GetLocalCredentialsResultCallback callback),
              (override));
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_SERVICE_CONTROLLER_H_
