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
#include "third_party/nearby/presence/credential.h"
#include "third_party/nearby/presence/presence_identity.h"

namespace location {
namespace nearby {
namespace api {

using ::nearby::presence::PresenceIdentity;
using ::nearby::presence::PrivateCredential;
using ::nearby::presence::PublicCredential;

enum class CredentialOperationStatus {
  kFailed = 0,
  kSucceeded = 1,
};

struct CredentialSelector {
  std::string account_name;
  PresenceIdentity::IdentityType identity_type;
};

struct SaveCredentialCallback {
  std::function<void(CredentialOperationStatus)> credentials_saved_cb;
};

struct GetPrivateCredentialCallback {
  std::function<void(ExceptionOr<std::vector<PrivateCredential>>)>
      credentials_fetched_cb;
};

struct GetPublicCredentialCallback {
  std::function<void(ExceptionOr<std::vector<PublicCredential>>)>
      credentials_fetched_cb;
};

/*
 * This class specifies the virtual functions for native platforms to implement.
 */
class CredentialStorage {
 public:
  CredentialStorage() = default;
  virtual ~CredentialStorage() = default;
  // Used for
  // 1. Save/update private creds after (re)generate credentials invoked by
  // manager app. (public_credentials will be empty for this case); Or
  // 2. Update remote public creds after manager app downloaded a new batch of
  // remote public creds and save to local storage. (private_credentials will
  // be empty for this case).
  // account_name will be used as the key in both options, and both would
  // overwrite the previous credentials if there already exists credentials for
  // that given account_name.
  virtual void SaveCredentials(
      std::string account_name,
      std::vector<PrivateCredential> private_credentials,
      std::vector<PublicCredential> public_credentials,
      SaveCredentialCallback callback);

  // Used to fetch private creds when broadcasting.
  virtual void GetPrivateCredentials(CredentialSelector credential_selector,
                                     GetPrivateCredentialCallback callback);

  // Used to fetch remote public creds when scanning.
  virtual void GetPublicCredentials(CredentialSelector credential_selector,
                                    GetPublicCredentialCallback callback);
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_STORAGE_H_
