// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_ACCOUNT_MANAGER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_ACCOUNT_MANAGER_H_

#include <optional>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/base/observer_list.h"
#include "internal/platform/implementation/account_manager.h"

namespace nearby {

// A fake implementation of FakeAccountManager, along with a fake
// factory, to be used in tests.
class FakeAccountManager : public AccountManager {
 public:
  FakeAccountManager() = default;
  ~FakeAccountManager() override = default;

  std::optional<Account> GetCurrentAccount() override;

  void Login(
      absl::AnyInvocable<void(Account)> login_success_callback,
      absl::AnyInvocable<void(absl::Status)> login_failure_callback) override;

  void Login(
      absl::string_view client_id, absl::string_view client_secret,
      absl::AnyInvocable<void(Account)> login_success_callback,
      absl::AnyInvocable<void(absl::Status)> login_failure_callback) override;

  void Logout(absl::AnyInvocable<void(absl::Status)> logout_callback) override;

  bool GetAccessToken(
      absl::string_view account_id,
      absl::AnyInvocable<void(absl::string_view)> success_callback,
      absl::AnyInvocable<void(absl::Status)> failure_callback) override;
  std::pair<std::string, std::string> GetOAuthClientCredential() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Methods to set API response.
  void SetAccount(std::optional<Account> account);

  void SetLogoutSuccess(bool is_logout_success) {
    is_logout_success_ = is_logout_success;
  }

  void NotifyCredentialError() {
    NotifyLogout(account_->id, /*credential_error=*/true);
  }

 private:
  // Updates current username to preference.
  void UpdateCurrentUser(absl::string_view current_user);
  void ClearCurrentUser();
  void NotifyLogin(absl::string_view account_id);
  void NotifyLogout(absl::string_view account_id, bool credential_error);

  // Login will fail when account_ is empty.
  std::optional<Account> account_;

  // Logout will fail when is_logout_success_ is false;
  bool is_logout_success_ = true;
  nearby::ObserverList<Observer> observers_;
  std::optional<std::string> user_name_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_ACCOUNT_MANAGER_H_
