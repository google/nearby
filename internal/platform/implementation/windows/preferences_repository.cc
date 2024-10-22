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

#include <exception>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <optional>

#include "absl/synchronization/mutex.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "internal/base/files.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {
using json = ::nlohmann::json;

constexpr char kPreferencesFileName[] = "preferences.json";
constexpr char kPreferencesBackupFileName[] = "preferences_bak.json";

}  // namespace

json PreferencesRepository::LoadPreferences() {
  absl::MutexLock lock(&mutex_);
  std::optional<json> preferences = AttemptLoad();
  if (preferences.has_value()) {
    // The top level root should be an object, if it's not then something went
    // wrong or the file was corrupted.
    if (!preferences.value().is_object()) {
      LOG(ERROR) << "Preferences loaded was not a valid object: "
                 << preferences.value().dump(4);

      return json::object();
    }

    return preferences.value();
  }

  LOG(ERROR) << "Could not load preferences file, trying backup.";

  // In the future we should switch to using a transaction log or another
  // stable method which doesn't pose a risk of losing settings
  preferences = RestoreFromBackup();
  if (preferences.has_value()) {
    LOG(ERROR) << "Successfully recovered from backup.";
    return preferences.value();
  }

  LOG(ERROR) << "Failed to load preferences file from back up.";

  return json::object();
}

bool PreferencesRepository::SavePreferences(json preferences) {
  absl::MutexLock lock(&mutex_);
  try {
    std::filesystem::path path = path_;
    if (!nearby::sharing::FileExists(path) &&
        !nearby::sharing::CreateDirectories(path)) {
      LOG(ERROR) << "Failed to create preferences path.";
      return false;
    }

    std::filesystem::path full_name = path / kPreferencesFileName;
    std::filesystem::path full_name_backup = path / kPreferencesBackupFileName;

    // Create a backup without moving the bytes on disk
    if (nearby::sharing::FileExists(full_name)) {
      LOG(INFO) << "Making backup of preferences file.";
      if (!nearby::sharing::Rename(full_name, full_name_backup)) {
        LOG(ERROR) << "Failed to rename preferences backup file.";
      }
    }

    std::ofstream preferences_file(full_name);
    preferences_file << preferences;
    preferences_file.close();

    // Make sure the file wasn't saved in a corrupted state
    if (!AttemptLoad().has_value()) {
      LOG(ERROR) << "Preferences saved to disk in corrupted state. "
                    "Restoring from backup.";

      if (!RestoreFromBackup().has_value()) {
        LOG(ERROR) << "Failed to restore preferences file.";
        return false;
      }
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to save preferences file: " << e.what();
    return false;
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
    return false;
  }

  return true;
}

std::optional<json> PreferencesRepository::AttemptLoad() {
  std::filesystem::path path = path_;
  std::filesystem::path full_name = path / kPreferencesFileName;
  if (!nearby::sharing::DirectoryExists(path) ||
      !nearby::sharing::FileExists(full_name)) {
    return std::nullopt;
  }

  try {
    std::ifstream preferences_file(full_name);
    if (!preferences_file.good()) {
      return std::nullopt;
    }

    json preferences = json::parse(preferences_file, nullptr, false);
    preferences_file.close();

    if (preferences.is_discarded()) {
      LOG(ERROR) << "Preferences file corrupted.";
      return std::nullopt;
    }

    return preferences;
  } catch (const std::exception& e) {
    LOG(ERROR) << "Exception while loading preferences: " << e.what();
    return std::nullopt;
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
    return std::nullopt;
  }
}

std::optional<json> PreferencesRepository::RestoreFromBackup() {
  std::filesystem::path path = path_;
  std::filesystem::path full_name = path / kPreferencesFileName;
  std::filesystem::path full_name_backup = path / kPreferencesBackupFileName;

  if (!nearby::sharing::FileExists(full_name_backup)) {
    LOG(WARNING) << "Backup requested but no backup preferences file found.";
    return std::nullopt;
  }

  if (!nearby::sharing::Rename(full_name_backup, full_name)) {
    LOG(ERROR) << "Failed to rename preferences backup file.";
  }

  LOG(INFO) << "Attempting load from backup preferences.";
  return AttemptLoad();
}

}  // namespace windows
}  // namespace nearby
