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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_ACCOUNT_PROVIDER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_ACCOUNT_PROVIDER_H_

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace nearby {
struct AccessTokenCallback {
  absl::AnyInvocable<void(absl::StatusOr<absl::string_view>)> access_token_cb;
};

// Fetches access tokens for use with certain APIs that require them.
class AccountProvider {
 public:
  virtual ~AccountProvider() = default;
  virtual void GetAccountAccessToken(AccessTokenCallback callback) = 0;
};
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_ACCOUNT_PROVIDER_H_
