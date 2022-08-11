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
#include <set>
#include <string>
#include <vector>

#include "internal/platform/exception.h"
#include "internal/proto/credential.pb.h"
#include "presence/presence_identity.h"

namespace location {
namespace nearby {
namespace api {

using ::nearby::internal::IdentityType;
using ::nearby::internal::PrivateCredential;
using ::nearby::internal::PublicCredential;

enum class CredentialOperationStatus {
  kFailed = 0,
  kSucceeded = 1,
};

struct CredentialSelector {
  std::string account_name;
  IdentityType identity_type;
};

enum PublicCredentialType {
  kLocalPublicCredential = 1,
  kRemotePublicCredential = 2,
};

struct SaveCredentialsResultCallback {
  std::function<void(CredentialOperationStatus)> credentials_saved_cb;
};

struct GetPrivateCredentialsResultCallback {
  std::function<void(std::vector<PrivateCredential>)> credentials_fetched_cb;
  std::function<void(CredentialOperationStatus)> get_credentials_failed_cb;
};

struct GetPublicCredentialsResultCallback {
  std::function<void(std::vector<PublicCredential>)> credentials_fetched_cb;
  std::function<void(CredentialOperationStatus)> get_credentials_failed_cb;
};

/*
 * This class specifies the virtual functions for native platforms to implement.
 */
class CredentialStorage {
 public:
  CredentialStorage() = default;
  virtual ~CredentialStorage() = default;
  // Used for
  // 1. Save private creds after (re)generate credentials invoked by manager app
  // 2. Update remote public creds after manager app update the public creds.
  // Skip the save/update if the provided vector is empty.
  // Another way is to break this into two APIs for save and update separately.
  virtual void SavePrivateCredentials(
      std::string account_name,
      std::vector<PrivateCredential> private_credentials,
      SaveCredentialsResultCallback callback);

  virtual void SavePublicCredentials(
      std::string account_name,
      std::vector<PublicCredential> public_credentials,
      PublicCredentialType public_credential_type,
      SaveCredentialsResultCallback callback);

  // Used to fetch private creds when broadcasting.
  virtual void GetPrivateCredentials(
      CredentialSelector credential_selector,
      GetPrivateCredentialsResultCallback callback);

  // Used to fetch remote public creds when scanning.
  virtual void GetPublicCredentials(
      CredentialSelector credential_selector,
      PublicCredentialType public_credential_type,
      GetPublicCredentialsResultCallback callback);
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_STORAGE_H_
