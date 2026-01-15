// Copyright 2021-2023 Google LLC
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

#include "internal/platform/implementation/linux/preferences_manager.h"

#include <stdint.h>

#include <fstream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/platform/logging.h"

// If nlohmann gives clangd errors, uncomment this when working on devcontainers. it helps. remove when building.
// build will fail.
// #include "external/nlohmann_json+/single_include/nlohmann/json.hpp"

namespace nearby {
namespace linux {
namespace {
using json = ::nlohmann::json;
constexpr absl::string_view kPreferencesFilePath = "Google/Nearby/Sharing";

class PreferencesManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_dir_ = Files::GetTemporaryDirectory();
    preferences_path_ = temp_dir_.GetPath();
    if (Files::FileExists(FilePath(preferences_path_)))
    {
      Files::RemoveFile(FilePath(preferences_path_));
    }
  }

  void TearDown() override {
    // Clean up any created preference files
    FilePath pref_file = temp_dir_;
    pref_file.append(FilePath("preferences.json"));
    if (Files::FileExists(pref_file)) {
      Files::RemoveFile(pref_file);
    }
  }

  FilePath temp_dir_;
  std::string preferences_path_;
};

}  // namespace

TEST_F(PreferencesManagerTest, CorruptedConfigFile) {
  FilePath preferencesPath = temp_dir_;
  preferencesPath.append(FilePath("preferences.json"));
  std::ofstream output_stream{preferencesPath.GetPath()};
  output_stream << "CORRUPTED" << std::endl;
  output_stream.close();

  LOG(INFO) << "Loading preferences from: " << temp_dir_.ToString();
  PreferencesManager pm(preferences_path_);
  EXPECT_EQ(pm.GetInteger("data", 100), 100);
}

TEST_F(PreferencesManagerTest, ValidConfigFile) {
  FilePath preferencesPath = temp_dir_;
  preferencesPath.append(FilePath("preferences.json"));
  std::ofstream output_stream{preferencesPath.GetPath()};
  output_stream << "{\"data\":8, \"name\": \"Valid\"}" << std::endl;
  output_stream.close();

  LOG(INFO) << "Loading preferences from: " << temp_dir_.ToString();
  PreferencesManager pm(preferences_path_);
  EXPECT_EQ(pm.GetInteger("data", 100), 8);
}

TEST(PreferencesManager, SetAndGetBoolean) {
  std::string bool_key = "bool_key";
  PreferencesManager pm(kPreferencesFilePath);
  EXPECT_TRUE(pm.GetBoolean(bool_key, true));
  pm.SetBoolean(bool_key, true);
  EXPECT_TRUE(pm.GetBoolean(bool_key, false));
}

TEST(PreferencesManager, SetAndGetInt) {
  std::string int_key = "int_key";
  PreferencesManager pm(kPreferencesFilePath);
  EXPECT_EQ(pm.GetInteger(int_key, 1234), 1234);
  pm.SetInteger(int_key, 6789);
  EXPECT_EQ(pm.GetInteger(int_key, 0), 6789);
}

TEST(PreferencesManager, SetAndGetInt64) {
  std::string int64_key = "int64_key";
  PreferencesManager pm(kPreferencesFilePath);
  EXPECT_EQ(pm.GetInt64(int64_key, 1234), 1234);
  pm.SetInt64(int64_key, 56789);
  EXPECT_EQ(pm.GetInt64(int64_key, 0), 56789);
}

TEST(PreferencesManager, SetAndGetString) {
  std::string string_key = "string_key";
  PreferencesManager pm(kPreferencesFilePath);
  EXPECT_EQ(pm.GetString(string_key, "abcd"), "abcd");
  pm.SetString(string_key, "this is a test string");
  EXPECT_EQ(pm.GetString(string_key, ""), "this is a test string");
}

TEST(PreferencesManager, SetAndGetTime) {
  std::string time_key = "time_key";
  PreferencesManager pm(kPreferencesFilePath);
  absl::Time time = absl::Now();
  EXPECT_EQ(pm.GetTime(time_key, time), time);
  pm.SetTime(time_key, time);
  absl::Time ret = pm.GetTime(time_key, absl::Now());
  EXPECT_EQ(absl::ToUnixNanos(ret), absl::ToUnixNanos(time));
}

TEST(PreferencesManager, MultipleSetAndGetString) {
  std::string string1_key = "string1_key";
  PreferencesManager pm(kPreferencesFilePath);
  pm.SetString(string1_key, "this is first string");
  pm.SetString(string1_key, "this is second string");
  EXPECT_EQ(pm.GetString(string1_key, ""), "this is second string");
}

TEST(PreferencesManager, SetAndGetValue) {
  std::string value_key = "value_key";
  PreferencesManager pm(kPreferencesFilePath);
  json value = {{"key1", "value1"}, {"key2", "value2"}};
  EXPECT_TRUE(pm.Get(value_key, json()).empty());
  pm.Set(value_key, value);
  auto result = pm.Get(value_key, json());
  ASSERT_FALSE(result.empty());
  auto val = result["key2"];
  EXPECT_EQ(val.get<std::string>(), "value2");
}

TEST(PreferencesManager, SetAndGetBooleanArray) {
  std::string bool_array_key = "bool_array_key";
  auto pm = PreferencesManager(kPreferencesFilePath);
  auto default_result =
      pm.GetBooleanArray(bool_array_key, absl::Span<const bool>({true}));
  EXPECT_EQ(default_result[0], true);
  pm.SetBooleanArray(bool_array_key,
                     absl::Span<const bool>({true, false, false, true, true}));
  auto result =
      pm.GetBooleanArray(bool_array_key, absl::Span<const bool>({true}));
  EXPECT_EQ(result[2], false);
  EXPECT_EQ(result[3], true);
}

TEST(PreferencesManager, SetAndGetIntArray) {
  std::string int_array_key = "int_array_key";
  auto pm = PreferencesManager(kPreferencesFilePath);
  auto result = pm.GetIntegerArray(int_array_key, std::vector<int>{5, 6});
  EXPECT_EQ(result[1], 6);
  pm.SetIntegerArray(int_array_key, std::vector<int>{1, 7, 4, 10, 12});
  result = pm.GetIntegerArray(int_array_key, std::vector<int>{11, 17, 14, 110});
  EXPECT_EQ(result[3], 10);
}

TEST(PreferencesManager, SetAndGetInt64Array) {
  std::string int64_array_key = "int64_array_key";
  auto pm = PreferencesManager(kPreferencesFilePath);
  auto result = pm.GetInt64Array(int64_array_key, std::vector<int64_t>{99});
  EXPECT_EQ(result[0], 99);
  pm.SetInt64Array(int64_array_key, std::vector<int64_t>{16, 7, 64, 100, 12});
  result = pm.GetInt64Array(int64_array_key, std::vector<int64_t>{1, 5, 6, 12});
  EXPECT_EQ(result[3], 100);
  EXPECT_EQ(result[4], 12);
}

TEST(PreferencesManager, SetAndGetStringArray) {
  std::string string_array_key = "string_array_key";
  auto pm = PreferencesManager(kPreferencesFilePath);
  auto result = pm.GetStringArray(string_array_key,
                                  std::vector<std::string>{"value", "morning"});
  EXPECT_EQ(result[1], "morning");
  pm.SetStringArray(
      string_array_key,
      std::vector<std::string>{"one", "two", "three", "four", "five"});
  result = pm.GetStringArray(string_array_key,
                             std::vector<std::string>{"good", "morning"});
  EXPECT_EQ(result[3], "four");
}

TEST(PreferencesManager, RemoveKey) {
  std::string string_key = "string_key";
  auto pm = PreferencesManager(kPreferencesFilePath);
  pm.SetString(string_key, "remove key");
  pm.Remove(string_key);
  auto result = pm.GetString(string_key, "default key");
  EXPECT_EQ(result, "default key");
}

}  // namespace linux
}  // namespace nearby


