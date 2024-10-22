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

#include "internal/platform/implementation/windows/preferences_manager.h"

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "internal/base/files.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/preferences_manager.h"
#include "internal/platform/implementation/windows/preferences_repository.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {
using json = ::nlohmann::json;
}  // namespace

PreferencesManager::PreferencesManager(absl::string_view file_path)
    : api::PreferencesManager(file_path) {
  std::optional<std::filesystem::path> path =
      nearby::api::ImplementationPlatform::CreateDeviceInfo()
          ->GetLocalAppDataPath();
  if (!path.has_value()) {
    path = nearby::sharing::GetTemporaryDirectory().value_or(
        nearby::sharing::CurrentDirectory());
  }

  std::filesystem::path full_path = *path / std::string(file_path);
  preferences_repository_ =
      std::make_unique<PreferencesRepository>(full_path.string());
  value_ = preferences_repository_->LoadPreferences();
}

bool PreferencesManager::Set(absl::string_view key, const json& value) {
  absl::MutexLock lock(&mutex_);
  return SetValue(key, value);
}

bool PreferencesManager::SetBoolean(absl::string_view key, bool value) {
  absl::MutexLock lock(&mutex_);
  return SetValue(key, value);
}

bool PreferencesManager::SetInteger(absl::string_view key, int value) {
  absl::MutexLock lock(&mutex_);
  return SetValue(key, value);
}

bool PreferencesManager::SetInt64(absl::string_view key, int64_t value) {
  absl::MutexLock lock(&mutex_);
  return SetValue(key, value);
}

bool PreferencesManager::SetString(absl::string_view key,
                                   absl::string_view value) {
  absl::MutexLock lock(&mutex_);
  return SetValue(key, absl::StrCat(value));
}

bool PreferencesManager::SetBooleanArray(absl::string_view key,
                                         absl::Span<const bool> value) {
  absl::MutexLock lock(&mutex_);
  return SetArrayValue(key, value);
}

bool PreferencesManager::SetIntegerArray(absl::string_view key,
                                         absl::Span<const int> value) {
  absl::MutexLock lock(&mutex_);
  return SetArrayValue(key, value);
}

bool PreferencesManager::SetInt64Array(absl::string_view key,
                                       absl::Span<const int64_t> value) {
  absl::MutexLock lock(&mutex_);
  return SetArrayValue(key, value);
}

bool PreferencesManager::SetStringArray(absl::string_view key,
                                        absl::Span<const std::string> value) {
  absl::MutexLock lock(&mutex_);
  return SetArrayValue(key, value);
}

bool PreferencesManager::SetTime(absl::string_view key, absl::Time value) {
  // Save time as nanos
  absl::MutexLock lock(&mutex_);
  int64_t tt = absl::ToUnixNanos(value);
  if (value_[absl::StrCat(key)] == tt) {
    return false;
  }

  value_[absl::StrCat(key)] = tt;
  return Commit();
}

// Get JSON value.
json PreferencesManager::Get(absl::string_view key,
                             const json& default_value) const {
  absl::MutexLock lock(&mutex_);
  return GetValue(key, default_value);
}

bool PreferencesManager::GetBoolean(absl::string_view key,
                                    bool default_value) const {
  absl::MutexLock lock(&mutex_);
  return GetValue(key, default_value);
}

int PreferencesManager::GetInteger(absl::string_view key,
                                   int default_value) const {
  absl::MutexLock lock(&mutex_);
  return GetValue(key, default_value);
}

int64_t PreferencesManager::GetInt64(absl::string_view key,
                                     int64_t default_value) const {
  absl::MutexLock lock(&mutex_);
  return GetValue(key, default_value);
}

std::string PreferencesManager::GetString(
    absl::string_view key, const std::string& default_value) const {
  absl::MutexLock lock(&mutex_);
  return GetValue(key, default_value);
}

std::vector<bool> PreferencesManager::GetBooleanArray(
    absl::string_view key, absl::Span<const bool> default_value) const {
  absl::MutexLock lock(&mutex_);
  return GetArrayValue(key, default_value);
}

std::vector<int> PreferencesManager::GetIntegerArray(
    absl::string_view key, absl::Span<const int> default_value) const {
  absl::MutexLock lock(&mutex_);
  return GetArrayValue(key, default_value);
}

std::vector<int64_t> PreferencesManager::GetInt64Array(
    absl::string_view key, absl::Span<const int64_t> default_value) const {
  absl::MutexLock lock(&mutex_);
  return GetArrayValue(key, default_value);
}

std::vector<std::string> PreferencesManager::GetStringArray(
    absl::string_view key, absl::Span<const std::string> default_value) const {
  absl::MutexLock lock(&mutex_);
  return GetArrayValue(key, default_value);
}

absl::Time PreferencesManager::GetTime(absl::string_view key,
                                       absl::Time default_value) const {
  absl::MutexLock lock(&mutex_);
  auto result = value_.find(absl::StrCat(key));
  if (result == value_.end()) {
    return default_value;
  }

  return absl::FromUnixNanos(result->get<int64_t>());
}

// Removes preferences
void PreferencesManager::Remove(absl::string_view key) {
  absl::MutexLock lock(&mutex_);
  value_.erase(absl::StrCat(key));
}

// Private methods

// Writes data to storage.
bool PreferencesManager::Commit() {
  if (!preferences_repository_->SavePreferences(value_)) {
    LOG(ERROR) << "Failed to save preference." << std::endl;
    return false;
  }
  return true;
}

bool PreferencesManager::SetValue(absl::string_view key, const json& value) {
  if (!value_.is_object()) {
    LOG(ERROR) << "Preferences is no longer an object! value_="
               << value_.dump(4);
    value_ = json::object();
  }

  if (value_[absl::StrCat(key)] == value) {
    return false;
  }

  value_[absl::StrCat(key)] = value;
  return Commit();
}

template <typename T>
T PreferencesManager::GetValue(absl::string_view key,
                               const T& default_value) const {
  if (!value_.is_object()) {
    LOG(ERROR) << "Preferences is no longer an object! value_="
               << value_.dump(4);
    return default_value;
  }

  auto it = value_.find(absl::StrCat(key));
  if (it == value_.end()) {
    return default_value;
  }
  return it->get<T>();
}

template <typename T>
bool PreferencesManager::SetArrayValue(absl::string_view key,
                                       absl::Span<const T> value) {
  if (!value_.is_object()) {
    LOG(ERROR) << "Preferences is no longer an object! value_="
               << value_.dump(4);
    value_ = json::object();
  }

  json array_value = json::array();
  for (const T& item_value : value) {
    array_value.push_back(item_value);
  }

  if (value_[absl::StrCat(key)] == array_value) {
    return false;
  }

  value_[absl::StrCat(key)] = array_value;
  return Commit();
}

template <typename T>
std::vector<T> PreferencesManager::GetArrayValue(
    absl::string_view key, absl::Span<const T> default_value) const {
  std::vector<T> result;

  if (!value_.is_object()) {
    LOG(ERROR) << "Preferences is no longer an object! value_="
               << value_.dump(4);

    for (const T& value : default_value) {
      result.push_back(value);
    }
    return result;
  }

  auto array_value = value_.find(absl::StrCat(key));
  if (array_value == value_.end() || !array_value->is_array()) {
    for (const T& value : default_value) {
      result.push_back(value);
    }
    return result;
  }

  auto it = array_value->begin();
  while (it != array_value->end()) {
    result.push_back(it->get<T>());
    ++it;
  }

  return result;
}

}  // namespace windows
}  // namespace nearby
