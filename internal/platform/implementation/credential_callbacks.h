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

#include <ostream>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "internal/proto/credential.pb.h"
#include "internal/proto/local_credential.pb.h"

namespace nearby {
namespace presence {

struct CredentialSelector {
  std::string manager_app_id;
  std::string account_name;
  ::nearby::internal::IdentityType identity_type;

  template <typename Sink>
  friend void AbslStringify(Sink& sink,
                            const CredentialSelector& credential_selector) {
    absl::Format(&sink, "CredentialSelector(%v, %v, IdentityType(%v))",
                 credential_selector.manager_app_id,
                 credential_selector.account_name,
                 static_cast<int>(credential_selector.identity_type));
  }
  template <typename H>
  friend H AbslHashValue(H h, const CredentialSelector& selector) {
    return H::combine(std::move(h), selector.manager_app_id,
                      selector.account_name, selector.identity_type);
  }
  friend bool operator==(const CredentialSelector& a,
                         const CredentialSelector& b) {
    return a.manager_app_id == b.manager_app_id &&
           a.account_name == b.account_name &&
           a.identity_type == b.identity_type;
  }
};

enum class PublicCredentialType {
  kLocalPublicCredential = 1,
  kRemotePublicCredential = 2,
};

struct SaveCredentialsResultCallback {
  absl::AnyInvocable<void(absl::Status)> credentials_saved_cb;
};

struct GenerateCredentialsResultCallback {
  absl::AnyInvocable<void(
      absl::StatusOr<std::vector<nearby::internal::SharedCredential>>)>
      credentials_generated_cb;
};

struct UpdateRemotePublicCredentialsCallback {
  absl::AnyInvocable<void(absl::Status)> credentials_updated_cb;
};

struct GetLocalCredentialsResultCallback {
  absl::AnyInvocable<void(
      absl::StatusOr<std::vector<nearby::internal::LocalCredential>>)>
      credentials_fetched_cb;
};

struct GetPublicCredentialsResultCallback {
  absl::AnyInvocable<void(
      absl::StatusOr<std::vector<nearby::internal::SharedCredential>>)>
      credentials_fetched_cb;
};

inline std::ostream& operator<<(std::ostream& os,
                                const PublicCredentialType& credential_type) {
  return os << "PublicCredentialType(" << static_cast<int>(credential_type)
            << ")";
}

inline std::ostream& operator<<(std::ostream& os,
                                const CredentialSelector& credential_selector) {
  return os << "CredentialSelector(" << credential_selector.manager_app_id
            << ", " << credential_selector.account_name << ", IdentityType("
            << static_cast<int>(credential_selector.identity_type) << "))";
}

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_CREDENTIAL_CALLBACKS_H_
