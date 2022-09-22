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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_CALLBACKS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_CALLBACKS_H_

#include <functional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace presence {

enum class CredentialOperationStatus {
  kUnknown = 0,
  kFailed = 1,
  kSucceeded = 2,
};

struct CredentialSelector {
  std::string manager_app_id;
  std::string account_name;
  ::nearby::internal::IdentityType identity_type;
};

enum PublicCredentialType {
  kLocalPublicCredential = 1,
  kRemotePublicCredential = 2,
};

struct GenerateCredentialsCallback {
  std::function<void(std::vector<nearby::internal::PublicCredential>)>
      credentials_generated_cb;
};

struct UpdateRemotePublicCredentialsCallback {
  std::function<void(CredentialOperationStatus)> credentials_updated_cb;
};

struct GetPrivateCredentialsResultCallback {
  std::function<void(std::vector<::nearby::internal::PrivateCredential>)>
      credentials_fetched_cb;
  std::function<void(CredentialOperationStatus)> get_credentials_failed_cb;
};

struct GetPublicCredentialsResultCallback {
  std::function<void(std::vector<::nearby::internal::PublicCredential>)>
      credentials_fetched_cb;
  std::function<void(CredentialOperationStatus)> get_credentials_failed_cb;
};
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_CALLBACKS_H_
