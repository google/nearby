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

#ifndef PLATFORM_IMPLEMENTATION_G3_PREFERENCES_MANAGER_H_
#define PLATFORM_IMPLEMENTATION_G3_PREFERENCES_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "internal/platform/implementation/g3/preferences_repository.h"
#include "internal/platform/implementation/preferences_manager.h"

namespace nearby {
namespace g3 {

// Sets and gets preference settings from the application.
// Preferences are persistent storage for application settings, it is key/value
// based settings. Application components can observe the interested preference
// change by the observer.
class PreferencesManager : public api::PreferencesManager {
 public:
  explicit PreferencesManager(absl::string_view path);

  // Sets values

  bool Set(absl::string_view key, const nlohmann::json& value) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool SetBoolean(absl::string_view key, bool value) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool SetInteger(absl::string_view key, int value) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool SetInt64(absl::string_view key, int64_t value) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool SetString(absl::string_view key, absl::string_view value) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool SetBooleanArray(absl::string_view key,
                       absl::Span<const bool> value) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool SetIntegerArray(absl::string_view key,
                       absl::Span<const int> value) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool SetInt64Array(absl::string_view key,
                     absl::Span<const int64_t> value) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool SetStringArray(absl::string_view key,
                      absl::Span<const std::string> value) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool SetTime(absl::string_view key, absl::Time value) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Gets values
  nlohmann::json Get(absl::string_view key,
                     const nlohmann::json& default_value) const override
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool GetBoolean(absl::string_view key, bool default_value) const override
      ABSL_LOCKS_EXCLUDED(mutex_);
  int GetInteger(absl::string_view key, int default_value) const override
      ABSL_LOCKS_EXCLUDED(mutex_);
  int64_t GetInt64(absl::string_view key, int64_t default_value) const override
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::string GetString(absl::string_view key,
                        const std::string& default_value) const override
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::vector<bool> GetBooleanArray(absl::string_view key,
                                    absl::Span<const bool> default_value)
      const override ABSL_LOCKS_EXCLUDED(mutex_);
  std::vector<int> GetIntegerArray(
      absl::string_view key, absl::Span<const int> default_value) const override
      ABSL_LOCKS_EXCLUDED(mutex_);
  std::vector<int64_t> GetInt64Array(absl::string_view key,
                                     absl::Span<const int64_t> default_value)
      const override ABSL_LOCKS_EXCLUDED(mutex_);
  std::vector<std::string> GetStringArray(
      absl::string_view key,
      absl::Span<const std::string> default_value) const override
      ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Time GetTime(absl::string_view key,
                     absl::Time default_value) const override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Removes preferences
  void Remove(absl::string_view key) override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // Writes data to storage.
  bool Commit() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool SetValue(absl::string_view key, const nlohmann::json& value)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  template <typename T>
  T GetValue(absl::string_view key, const T& default_value) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  template <typename T>
  bool SetArrayValue(absl::string_view key, absl::Span<const T> value)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  template <typename T>
  std::vector<T> GetArrayValue(absl::string_view key,
                               absl::Span<const T> default_value) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  nlohmann::json value_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<PreferencesRepository> preferences_repository_
      ABSL_GUARDED_BY(mutex_);

  mutable absl::Mutex mutex_;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_G3_PREFERENCES_MANAGER_H_
