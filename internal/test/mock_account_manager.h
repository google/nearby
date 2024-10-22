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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_TEST_MOCK_ACCOUNT_MANAGER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_TEST_MOCK_ACCOUNT_MANAGER_H_

#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/account_manager.h"

namespace nearby {

class MockAccountManager : public AccountManager {
 public:
  MOCK_METHOD(std::optional<Account>, GetCurrentAccount, (), (override));
  MOCK_METHOD(void, Login,
              (absl::AnyInvocable<void(Account)> login_success_callback,
               absl::AnyInvocable<void(absl::Status)> login_failure_callback),
              (override));
  MOCK_METHOD(void, Login,
              (absl::string_view client_id, absl::string_view client_secret,
               absl::AnyInvocable<void(Account)> login_success_callback,
               absl::AnyInvocable<void(absl::Status)> login_failure_callback),
              (override));
  MOCK_METHOD(void, Logout,
              (absl::AnyInvocable<void(absl::Status)> logout_callback),
              (override));
  MOCK_METHOD(bool, GetAccessToken,
              (absl::string_view account_id,
               absl::AnyInvocable<void(absl::string_view)> success_callback,
               absl::AnyInvocable<void(absl::Status)> failure_callback),
              (override));
  MOCK_METHOD((std::pair<std::string, std::string>), GetOAuthClientCredential,
              (), (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_MOCK_ACCOUNT_MANAGER_H_
