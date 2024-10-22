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

#ifndef PLATFORM_API_ACCOUNT_MANAGER_H_
#define PLATFORM_API_ACCOUNT_MANAGER_H_

#include <optional>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace nearby {

// AccountManager manages the accounts are used to access Nearby backend.
// In current design, AccountManager only support one active account.
class AccountManager {
 public:
  // Describes a Nearby account. The account class will have more properties
  // and methods in the future based on the new feature added.
  struct Account {
    std::string id;  // The unique identify of the account.
    std::string display_name;
    std::string family_name;
    std::string given_name;
    std::string picture_url;
    std::string email;
  };

  // Observes the activity of the account manager.
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnLoginSucceeded(absl::string_view account_id) = 0;
    // |credential_error| is true if the logout is due to critical auth error.
    virtual void OnLogoutSucceeded(absl::string_view account_id,
                                   bool credential_error) = 0;
  };

  virtual ~AccountManager() = default;

  // Gets current active account. If no login user, return std::nullopt.
  virtual std::optional<Account> GetCurrentAccount() = 0;

  // Initializes the login process for a Google account from 1P client.
  // |login_success_callback| is called when the login succeeded. Account
  // information is passed to callback.
  // |login_failure_callback| is called when the login fails.
  virtual void Login(
      absl::AnyInvocable<void(Account)> login_success_callback,
      absl::AnyInvocable<void(absl::Status)> login_failure_callback) = 0;

  // Initializes the login process for a Google account from an oauth client.
  // |client_id| GCP client_id of the client
  // |client_secret| GCP client_secret of the client
  // |login_success_callback| is called when the login succeeded. Account
  // information is passed to callback.
  // |login_failure_callback| is called when the login fails.
  virtual void Login(
      absl::string_view client_id, absl::string_view client_secret,
      absl::AnyInvocable<void(Account)> login_success_callback,
      absl::AnyInvocable<void(absl::Status)> login_failure_callback) = 0;

  // Logs out current active account. |logout_callback| is called when logout is
  // completed.
  virtual void Logout(
      absl::AnyInvocable<void(absl::Status)> logout_callback) = 0;

  // Gets access token for the active account.
  // |success_callback| is called when an access token is fetched successfully.
  // |failure_callback| is called when fetching an access token failed.
  //
  // Returns false if account_id is empty or callback is null.
  virtual bool GetAccessToken(
      absl::string_view account_id,
      absl::AnyInvocable<void(absl::string_view)> success_callback,
      absl::AnyInvocable<void(absl::Status)> failure_callback) = 0;

  // Returns a pair containing the client id and client secret used in the most
  // recent Login request.
  // If no current user is logged in, returns empty string for both.
  virtual std::pair<std::string, std::string> GetOAuthClientCredential() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace nearby

#endif  // PLATFORM_API_ACCOUNT_MANAGER_H_
