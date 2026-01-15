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

#include "internal/platform/implementation/linux/preferences_repository.h"

#include <fstream>
#include <memory>
#include <optional>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/device_info.h"
#include "internal/platform/implementation/platform.h"

namespace nearby {
namespace linux {
namespace {

using json = ::nlohmann::json;

constexpr char kPreferencesFileName[] = "preferences.json";
constexpr char kPreferencesBackupFileName[] = "preferences_bak.json";
constexpr char kPreferencesPath[] = "Google/Nearby/Sharing";

class PreferencesRepositoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    system_bus_ = getSystemBusConnection();
    device_info_ = std::make_unique<DeviceInfo>(system_bus_);
    
    std::optional<FilePath> app_data_path = device_info_->GetLocalAppDataPath();
    if (app_data_path.has_value()) {
      app_data_path_ = app_data_path.value();
      test_path_ = app_data_path_.append(FilePath(kPreferencesPath));
      test_path_str_ = test_path_.GetPath();
    }
  }

  void TearDown() override {
    // Clean up test files
    if (!test_path_str_.empty()) {
      FilePath pref_file = app_data_path_;
      pref_file.append(FilePath(kPreferencesFileName));
      if (Files::FileExists(pref_file)) {
        Files::RemoveFile(pref_file);
      }
      
      FilePath backup_file = test_path_;
      backup_file.append(FilePath(kPreferencesBackupFileName));
      if (Files::FileExists(backup_file)) {
        Files::RemoveFile(backup_file);
      }
    }
    device_info_.reset();
  }

  std::shared_ptr<sdbus::IConnection> system_bus_;
  std::unique_ptr<DeviceInfo> device_info_;
  FilePath app_data_path_;
  FilePath test_path_;
  std::string test_path_str_;
};

TEST(PreferencesRepository, LoadWithBadPath) {
  PreferencesRepository preferences_repository{"/invalid/path/a/b/c/d/e/f"};
  json result = preferences_repository.LoadPreferences();
  EXPECT_TRUE(result.empty());
}

TEST_F(PreferencesRepositoryTest, RecoverFromBadPreferences) {
  if (test_path_str_.empty()) {
    GTEST_SKIP() << "Cannot get app data path in test environment";
  }

  FilePath full_name = app_data_path_;
  full_name.append(FilePath(kPreferencesFileName));

  if (Files::FileExists(full_name)) {
    Files::RemoveFile(full_name);
  }

  std::ofstream pref_file(full_name.GetPath());
  pref_file << "\"Bad top level object\"";
  pref_file.close();

  PreferencesRepository preferences_repository{test_path_str_};
  EXPECT_EQ(preferences_repository.LoadPreferences(), json::object());
}

TEST_F(PreferencesRepositoryTest, SaveAndLoadPreferences) {
  if (test_path_str_.empty()) {
    GTEST_SKIP() << "Cannot get app data path in test environment";
  }

  FilePath full_name = app_data_path_;
  full_name.append(FilePath(kPreferencesFileName));

  if (Files::FileExists(full_name)) {
    Files::RemoveFile(full_name);
  }

  PreferencesRepository preferences_repository{test_path_str_};
  json data;
  data["key1"] = "value1";
  data["key2"] = "value2";
  EXPECT_TRUE(preferences_repository.SavePreferences(data));
  json result = preferences_repository.LoadPreferences();
  EXPECT_EQ(result.size(), 2);
  EXPECT_EQ(result["key1"], "value1");
  EXPECT_EQ(result["key2"], "value2");
  Files::RemoveFile(full_name);
}

TEST_F(PreferencesRepositoryTest, LoadFromBackup) {
  if (test_path_str_.empty()) {
    GTEST_SKIP() << "Cannot get app data path in test environment";
  }

  FilePath full_name = app_data_path_;
  full_name.append(FilePath(kPreferencesFileName));
  FilePath full_name_backup = test_path_;
  full_name_backup.append(FilePath(kPreferencesBackupFileName));

  if (Files::FileExists(full_name)) {
    Files::RemoveFile(full_name);
  }

  if (Files::FileExists(full_name_backup)) {
    Files::RemoveFile(full_name_backup);
  }

  PreferencesRepository preferences_repository{test_path_str_};
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
  Files::RemoveFile(full_name);
  EXPECT_FALSE(Files::FileExists(full_name_backup));
}

TEST_F(PreferencesRepositoryTest, RecoverFromCorruption) {
  if (test_path_str_.empty()) {
    GTEST_SKIP() << "Cannot get app data path in test environment";
  }

  FilePath full_name = app_data_path_;
  full_name.append(FilePath(kPreferencesFileName));
  FilePath full_name_backup = test_path_;
  full_name_backup.append(FilePath(kPreferencesBackupFileName));

  if (Files::FileExists(full_name)) {
    Files::RemoveFile(full_name);
  }

  if (Files::FileExists(full_name_backup)) {
    Files::RemoveFile(full_name_backup);
  }

  PreferencesRepository preferences_repository{test_path_str_};
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
  Files::RemoveFile(full_name);
  EXPECT_FALSE(Files::FileExists(full_name_backup));
}

}  // namespace
}  // namespace linux
}  // namespace nearby
