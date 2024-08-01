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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_TEST_MOCK_ACCOUNT_OBSERVER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_TEST_MOCK_ACCOUNT_OBSERVER_H_

#include "gmock/gmock.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/account_manager.h"

namespace nearby {

class MockAccountObserver : public AccountManager::Observer {
 public:
  ~MockAccountObserver() override = default;

  MOCK_METHOD(void, OnLoginSucceeded, (absl::string_view account_id),
              (override));

  MOCK_METHOD(void, OnLogoutSucceeded,
              (absl::string_view account_id, bool credential_error),
              (override));
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_MOCK_ACCOUNT_OBSERVER_H_
