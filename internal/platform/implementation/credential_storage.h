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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_STORAGE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_STORAGE_H_

#include <functional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/proto/credential.pb.h"

namespace location {
namespace nearby {
namespace api {

/*
 * This class specifies the virtual functions for native platforms to implement.
 */
class CredentialStorage {
 public:
  virtual ~CredentialStorage() = default;
  // Used for
  // 1. Save private creds after (re)generate credentials invoked by manager app
  // 2. Update remote public creds after manager app update the public creds.
  // Skip the save/update if the provided vector is empty.
  // Another way is to break this into two APIs for save and update separately.
  virtual void SaveCredentials(
      absl::string_view manager_app_id, absl::string_view account_name,
      const std::vector<::nearby::internal::PrivateCredential>&
          private_credentials,
      const std::vector<::nearby::internal::PublicCredential>&
          public_credentials,
      ::nearby::presence::PublicCredentialType public_credential_type,
      ::nearby::presence::GenerateCredentialsCallback callback) = 0;

  // Used to fetch private creds when broadcasting.
  virtual void GetPrivateCredentials(
      const ::nearby::presence::CredentialSelector& credential_selector,
      ::nearby::presence::GetPrivateCredentialsResultCallback callback) = 0;

  // Used to fetch remote public creds when scanning.
  virtual void GetPublicCredentials(
      const ::nearby::presence::CredentialSelector& credential_selector,
      ::nearby::presence::PublicCredentialType public_credential_type,
      ::nearby::presence::GetPublicCredentialsResultCallback callback) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_STORAGE_H_
