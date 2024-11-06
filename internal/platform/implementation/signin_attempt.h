// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_SIGNIN_ATTEMPT_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_SIGNIN_ATTEMPT_H_

#include <string>

#include "absl/functional/any_invocable.h"
#include "internal/platform/implementation/account_info.h"
#include "internal/platform/implementation/auth_status.h"

namespace nearby {

class SigninAttempt {
 public:
  SigninAttempt() = default;
  virtual ~SigninAttempt() = default;

  // Starts a new sign-in attempt.
  // `callback` is called with the status of the request and the user_id if the
  // request is successful.
  // Returns the auth url if the request is successful.
  virtual std::string Start(
      absl::AnyInvocable<void(AuthStatus, const AccountInfo&)> callback) = 0;

  // Tears down the machinery set up to request auth tokens, including the HTTP
  // server.
  virtual void Close() = 0;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_SIGNIN_ATTEMPT_H_
