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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_STORAGE_H_
#define THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_STORAGE_H_

#include "internal/platform/implementation/credential_storage.h"

namespace location {
namespace nearby {
/*
 * The instance of CredentialStorage is owned by {@code CredentialManager}.
 * It's a wrapper on top of implementation/credential_storage.h to providing
 * credential storage operations for Nearby logic layer to invoke.
 */
class CredentialStorage {
 public:
  CredentialStorage() = default;
  ~CredentialStorage() = default;

 private:
  api::CredentialStorage credential_storage_;
};

}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_PRESENCE_IMPLEMENTATION_CREDENTIAL_STORAGE_H_
