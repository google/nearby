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

#ifndef LOCATION_NEARBY_SHARING_LIB_ACCOUNT_ACCOUNT_MANAGER_H_
#define LOCATION_NEARBY_SHARING_LIB_ACCOUNT_ACCOUNT_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace nearby::sharing {

class SigninAttempt;

// Stand-in for the Google-internal account manager interface, reconstructed
// from its uses in this repository. Implementations decide whether a Google
// account is available; without one, sharing runs in Everyone/hidden modes.
class AccountManager {
 public:
  struct Account {
    std::string id;
    std::string display_name;
    std::string given_name;
    std::string picture_url;
    std::string email;
  };

  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnLoginSucceeded(absl::string_view account_id) = 0;
    virtual void OnLogoutSucceeded(absl::string_view account_id,
                                   bool credential_error) = 0;
  };

  virtual ~AccountManager() = default;

  virtual std::optional<Account> GetCurrentAccount() = 0;
  virtual std::unique_ptr<SigninAttempt> Login(
      absl::string_view client_id, absl::string_view client_secret) = 0;
  virtual void Logout(absl::AnyInvocable<void(absl::Status)> logout_callback) = 0;
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace nearby::sharing

#endif  // LOCATION_NEARBY_SHARING_LIB_ACCOUNT_ACCOUNT_MANAGER_H_
