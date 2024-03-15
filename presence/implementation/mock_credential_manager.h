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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_CREDENTIAL_MANAGER_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_CREDENTIAL_MANAGER_H_

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "presence/implementation/credential_manager.h"

namespace nearby {
namespace presence {

class MockCredentialManager : public CredentialManager {
 public:
  MOCK_METHOD(
      void, GenerateCredentials,
      (const nearby::internal::DeviceIdentityMetaData& device_identity_metadata,
       absl::string_view manager_app_id,
       const std::vector<nearby::internal::IdentityType>& identity_types,
       int credential_life_cycle_days, int contiguous_copy_of_credentials,
       GenerateCredentialsResultCallback credentials_generated_cb),
      (override));
  MOCK_METHOD(void, UpdateRemotePublicCredentials,
              (absl::string_view manager_app_id, absl::string_view account_name,
               const std::vector<nearby::internal::SharedCredential>&
                   remote_public_creds,
               UpdateRemotePublicCredentialsCallback credentials_updated_cb),
              (override));
  MOCK_METHOD(void, UpdateLocalCredential,
              (const CredentialSelector& credential_selector,
               nearby::internal::LocalCredential credential,
               SaveCredentialsResultCallback result_callback),
              (override));
  MOCK_METHOD(void, GetLocalCredentials,
              (const CredentialSelector& credential_selector,
               GetLocalCredentialsResultCallback callback),
              (override));
  MOCK_METHOD(void, GetPublicCredentials,
              (const CredentialSelector& credential_selector,
               PublicCredentialType public_credential_type,
               GetPublicCredentialsResultCallback callback),
              (override));
  MOCK_METHOD(SubscriberId, SubscribeForPublicCredentials,
              (const CredentialSelector& credential_selector,
               PublicCredentialType public_credential_type,
               GetPublicCredentialsResultCallback callback),
              (override));
  MOCK_METHOD(void, UnsubscribeFromPublicCredentials, (SubscriberId id),
              (override));
  MOCK_METHOD(std::string, DecryptDeviceIdentityMetaData,
              (absl::string_view metadata_encryption_key,
               absl::string_view key_seed, absl::string_view metadata_string),
              (override));
  MOCK_METHOD(
      void, SetDeviceIdentityMetaData,
      (const ::nearby::internal::DeviceIdentityMetaData&
           device_identity_metadata,
       bool regen_credentials, absl::string_view manager_app_id,
       const std::vector<nearby::internal::IdentityType>& identity_types,
       int credential_life_cycle_days, int contiguous_copy_of_credentials,
       GenerateCredentialsResultCallback credentials_generated_cb),
      (override));
  MOCK_METHOD(::nearby::internal::DeviceIdentityMetaData,
              GetDeviceIdentityMetaData, (), (override));
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_MOCK_CREDENTIAL_MANAGER_H_
