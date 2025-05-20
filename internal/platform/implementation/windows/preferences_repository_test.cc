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

#include "internal/platform/implementation/windows/preferences_repository.h"

#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <optional>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {
namespace windows {
namespace {

using json = ::nlohmann::json;

constexpr char kPreferencesFileName[] = "preferences.json";
constexpr char kPreferencesBackupFileName[] = "preferences_bak.json";
constexpr char kPreferencesPath[] = "Google/Nearby/Sharing";

TEST(PreferencesRepository, LoadWithBadPath) {
  PreferencesRepository preferences_repository{"c:\\users\\a\\b\\c\\d\\e\\f"};
  json result = preferences_repository.LoadPreferences();
  EXPECT_TRUE(result.empty());
}

TEST(PreferencesRepository, RecoverFromBadPreferences) {
  std::optional<FilePath> app_data_path =
      api::ImplementationPlatform::CreateDeviceInfo()->GetLocalAppDataPath();
  ASSERT_TRUE(app_data_path.has_value());
  FilePath full_path = app_data_path->append(FilePath(kPreferencesPath));
  FilePath full_name = app_data_path->append(FilePath(kPreferencesFileName));

  if (std::filesystem::exists(full_name.GetPath())) {
    std::filesystem::remove(full_name.GetPath());
  }

  std::ofstream pref_file(full_name.GetPath());
  pref_file << "\"Bad top level object\"";
  pref_file.close();

  PreferencesRepository preferences_repository{full_path.ToString()};
  EXPECT_EQ(preferences_repository.LoadPreferences(), json::object());
}

TEST(PreferencesRepository, SaveAndLoadPreferences) {
  std::optional<FilePath> app_data_path =
      api::ImplementationPlatform::CreateDeviceInfo()->GetLocalAppDataPath();
  ASSERT_TRUE(app_data_path.has_value());
  FilePath full_path = app_data_path->append(FilePath(kPreferencesPath));
  FilePath full_name = app_data_path->append(FilePath(kPreferencesFileName));

  if (std::filesystem::exists(full_name.GetPath())) {
    std::filesystem::remove(full_name.GetPath());
  }

  PreferencesRepository preferences_repository{full_path.ToString()};
  json data;
  data["key1"] = "value1";
  data["key2"] = "value2";
  EXPECT_TRUE(preferences_repository.SavePreferences(data));
  json result = preferences_repository.LoadPreferences();
  EXPECT_EQ(result.size(), 2);
  EXPECT_EQ(result["key1"], "value1");
  EXPECT_EQ(result["key2"], "value2");
  std::filesystem::remove(full_name.GetPath());
}

TEST(PreferencesRepository, LoadFromBackup) {
  std::optional<FilePath> app_data_path =
      api::ImplementationPlatform::CreateDeviceInfo()->GetLocalAppDataPath();
  ASSERT_TRUE(app_data_path.has_value());
  FilePath full_path = app_data_path->append(FilePath(kPreferencesPath));
  FilePath full_name = app_data_path->append(FilePath(kPreferencesFileName));
  FilePath full_name_backup = full_path;
  full_name_backup.append(FilePath(kPreferencesBackupFileName));

  if (std::filesystem::exists(full_name.GetPath())) {
    std::filesystem::remove(full_name.GetPath());
  }

  if (std::filesystem::exists(full_name_backup.GetPath())) {
    std::filesystem::remove(full_name_backup.GetPath());
  }

  PreferencesRepository preferences_repository{full_path.ToString()};
  json data;
  data["key1"] = "value1";
  data["key2"] = "value2";

  std::ofstream backup_file(full_name_backup.GetPath());
  backup_file << data;
  backup_file.close();

  std::optional<json> result;
  result = preferences_repository.AttemptLoad();
  EXPECT_FALSE(result.has_value());
  result = preferences_repository.RestoreFromBackup();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value()["key1"], "value1");
  EXPECT_EQ(result.value()["key2"], "value2");
  std::filesystem::remove(full_name.GetPath());
  EXPECT_FALSE(std::filesystem::exists(full_name_backup.GetPath()));
}

TEST(PreferencesRepository, RecoverFromCorruption) {
  std::optional<FilePath> app_data_path =
      api::ImplementationPlatform::CreateDeviceInfo()->GetLocalAppDataPath();
  ASSERT_TRUE(app_data_path.has_value());
  FilePath full_path = app_data_path->append(FilePath(kPreferencesPath));
  FilePath full_name = app_data_path->append(FilePath(kPreferencesFileName));
  FilePath full_name_backup = full_path;
  full_name_backup.append(FilePath(kPreferencesBackupFileName));

  if (std::filesystem::exists(full_name.GetPath())) {
    std::filesystem::remove(full_name.GetPath());
  }

  if (std::filesystem::exists(full_name_backup.GetPath())) {
    std::filesystem::remove(full_name_backup.GetPath());
  }

  PreferencesRepository preferences_repository{full_path.ToString()};
  json data;
  data["key1"] = "value1";
  data["key2"] = "value2";

  std::ofstream preferences_file(full_name_backup.GetPath());
  preferences_file << data;
  preferences_file.close();

  std::ofstream backup_file(full_name.GetPath());
  backup_file << "[BAD JSON FILE]";
  backup_file.close();

  std::optional<json> result = preferences_repository.LoadPreferences();
  EXPECT_EQ(result.value()["key1"], "value1");
  EXPECT_EQ(result.value()["key2"], "value2");
  std::filesystem::remove(full_name.GetPath());
  EXPECT_FALSE(std::filesystem::exists(full_name_backup.GetPath()));
}

}  // namespace
}  // namespace windows
}  // namespace nearby
