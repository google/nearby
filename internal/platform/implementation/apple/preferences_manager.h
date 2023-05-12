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

#ifndef PLATFORM_IMPLEMENTATION_APPLE_PREFERENCES_MANAGER_H_
#define PLATFORM_IMPLEMENTATION_APPLE_PREFERENCES_MANAGER_H_

#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "nlohmann/json_fwd.hpp"
#include "internal/platform/implementation/preferences_manager.h"

namespace nearby::apple {

class PreferencesManager : public api::PreferencesManager {
 public:
  explicit PreferencesManager(absl::string_view path);

  // Sets values

  bool Set(absl::string_view key, const nlohmann::json& value) override;

  bool SetBoolean(absl::string_view key, bool value) override;
  bool SetInteger(absl::string_view key, int value) override;
  bool SetInt64(absl::string_view key, int64_t value) override;
  bool SetString(absl::string_view key, absl::string_view value) override;

  bool SetBooleanArray(absl::string_view key,
                       absl::Span<const bool> value) override;
  bool SetIntegerArray(absl::string_view key,
                       absl::Span<const int> value) override;
  bool SetInt64Array(absl::string_view key,
                     absl::Span<const int64_t> value) override;
  bool SetStringArray(absl::string_view key,
                      absl::Span<const std::string> value) override;

  bool SetTime(absl::string_view key, absl::Time value) override;

  // Gets values
  nlohmann::json Get(absl::string_view key,
                     const nlohmann::json& default_value) const override;

  bool GetBoolean(absl::string_view key, bool default_value) const override;
  int GetInteger(absl::string_view key, int default_value) const override;
  int64_t GetInt64(absl::string_view key, int64_t default_value) const override;
  std::string GetString(absl::string_view key,
                        const std::string& default_value) const override;

  std::vector<bool> GetBooleanArray(
      absl::string_view key,
      absl::Span<const bool> default_value) const override;
  std::vector<int> GetIntegerArray(
      absl::string_view key,
      absl::Span<const int> default_value) const override;
  std::vector<int64_t> GetInt64Array(
      absl::string_view key,
      absl::Span<const int64_t> default_value) const override;
  std::vector<std::string> GetStringArray(
      absl::string_view key,
      absl::Span<const std::string> default_value) const override;

  absl::Time GetTime(absl::string_view key,
                     absl::Time default_value) const override;

  // Removes preferences
  void Remove(absl::string_view key) override;
};

}  // namespace nearby::apple

#endif  // PLATFORM_IMPLEMENTATION_APPLE_PREFERENCES_MANAGER_H_
