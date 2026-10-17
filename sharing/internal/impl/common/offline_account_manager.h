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
#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_COMMON_OFFLINE_ACCOUNT_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_COMMON_OFFLINE_ACCOUNT_MANAGER_H_

#include <memory>
#include <optional>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "location/nearby/sharing/lib/account/account_manager.h"
#include "location/nearby/sharing/lib/account/signin_attempt.h"

namespace nearby::sharing {

// An account manager that never has a Google account. Sign-in needs Google's
// OAuth client and identity service, which this build does not use, so
// sharing is limited to Everyone and hidden visibility.
class OfflineAccountManager : public AccountManager {
 public:
  std::optional<Account> GetCurrentAccount() override { return std::nullopt; }
  std::unique_ptr<SigninAttempt> Login(absl::string_view client_id,
                                       absl::string_view client_secret) override {
    return nullptr;
  }
  void Logout(absl::AnyInvocable<void(absl::Status)> logout_callback) override {
    logout_callback(absl::OkStatus());
  }
  // No account state ever changes, so observers are never notified.
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_IMPL_COMMON_OFFLINE_ACCOUNT_MANAGER_H_
