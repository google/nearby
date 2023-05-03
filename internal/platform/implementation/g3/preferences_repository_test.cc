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

#include "internal/platform/implementation/g3/preferences_repository.h"

#include <filesystem>  // NOLINT(build/c++17)
#include <string>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

namespace nearby {
namespace platform {
namespace g3 {
namespace {
using json = ::nlohmann::json;
}  // namespace

TEST(Preferences, TestSaveAndGetPreferences) {
  PreferencesRepository preferences_repository{
      std::filesystem::temp_directory_path().string()};
  std::string string_key = "string_value";
  std::string string_value = "hello world";
  std::string int_key = "int_value";
  json prefs = {{int_key, 345}, {string_key, string_value}};

  EXPECT_TRUE(preferences_repository.SavePreferences(prefs));

  auto result = preferences_repository.LoadPreferences();
  EXPECT_EQ(result[string_key].get<std::string>(), string_value);
  EXPECT_EQ(result[int_key].get<int>(), 345);
}

TEST(Preferences, TestMultipleSaveAndGetPreferences) {
  PreferencesRepository preferences_repository{
      std::filesystem::temp_directory_path().string()};
  std::string string_key = "string_value";
  std::string string_value = "hello world";
  std::string string_new_value = "again";
  std::string int_key = "int_value";
  json prefs = {{int_key, 345}, {string_key, string_value}};
  EXPECT_TRUE(preferences_repository.SavePreferences(prefs));

  json result = preferences_repository.LoadPreferences();
  EXPECT_EQ(result[string_key].get<std::string>(), string_value);
  prefs[string_key] = string_new_value;
  prefs[int_key] = 456;
  EXPECT_TRUE(preferences_repository.SavePreferences(prefs));
  result = preferences_repository.LoadPreferences();
  EXPECT_EQ(result[string_key].get<std::string>(), string_new_value);
  EXPECT_EQ(result[int_key].get<int>(), 456);
}

}  // namespace g3
}  // namespace platform
}  // namespace nearby
