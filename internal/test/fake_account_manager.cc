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

#include "internal/test/fake_account_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/implementation/signin_attempt.h"

namespace nearby {

std::optional<AccountManager::Account> FakeAccountManager::GetCurrentAccount() {
  return account_;
}

std::unique_ptr<SigninAttempt> FakeAccountManager::Login(
    absl::string_view client_id, absl::string_view client_secret) {
  return nullptr;
}

void FakeAccountManager::Logout(
    absl::AnyInvocable<void(absl::Status)> logout_callback) {
  if (is_logout_success_) {
    std::string account_id = account_->id;
    SetAccount(std::nullopt);
    NotifyLogout(account_id, /*credential_error=*/false);
    // Invoke callback after all operations have been performed since test cases
    // may rely on the callback for synchronization.
    logout_callback(absl::OkStatus());
    return;
  }

  logout_callback(absl::NotFoundError("No account login."));
}

bool FakeAccountManager::GetAccessToken(
    absl::string_view account_id,
    absl::AnyInvocable<void(absl::string_view)> success_callback,
    absl::AnyInvocable<void(absl::Status)> failure_callback) {
  if (!account_.has_value()) {
    failure_callback(absl::UnavailableError("No current user."));
    return false;
  }
  success_callback(account_id);
  return true;
}

std::pair<std::string, std::string>
FakeAccountManager::GetOAuthClientCredential() {
  return {"", ""};
}

void FakeAccountManager::SetAccount(std::optional<Account> account) {
  account_ = account;
}

void FakeAccountManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeAccountManager::RemoveObserver(Observer* observer) {
  if (!observers_.HasObserver(observer)) {
    return;
  }
  observers_.RemoveObserver(observer);
}

void FakeAccountManager::NotifyLogin(absl::string_view account_id) {
  for (const auto& observer : observers_.GetObservers()) {
    observer->OnLoginSucceeded(account_id);
  }
}

void FakeAccountManager::NotifyLogout(absl::string_view account_id,
                                      bool credential_error) {
  for (const auto& observer : observers_.GetObservers()) {
    observer->OnLogoutSucceeded(account_id, credential_error);
  }
}

}  // namespace nearby
