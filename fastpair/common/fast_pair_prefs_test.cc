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

#include "fastpair/common/fast_pair_prefs.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace prefs {
namespace {

using json = ::nlohmann::json;

constexpr char kPreferencesFilePath[] = "Google/Nearby/Preferences";

TEST(FastPairPreference, RegisterNearbyFastPairPrefs) {
  auto preferences_manager =
      std::make_unique<preferences::PreferencesManager>(kPreferencesFilePath);
  RegisterNearbyFastPairPrefs(preferences_manager.get());
  EXPECT_TRUE(
      preferences_manager->Get(kNearbyFastPairUsersName, json()).empty());

  json value = {{"current_user", "abc@gmail.com"}};
  preferences_manager->Set(kNearbyFastPairUsersName, value);
  auto result = preferences_manager->Get(kNearbyFastPairUsersName, json());
  ASSERT_FALSE(result.empty());
  auto val = result["current_user"];
  EXPECT_EQ(val.get<std::string>(), "abc@gmail.com");
}

}  // namespace
}  // namespace prefs
}  // namespace fastpair
}  // namespace nearby
