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

#import <Foundation/Foundation.h>

#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "nlohmann/json_fwd.hpp"
#include "internal/platform/implementation/preferences_manager.h"

namespace nearby::apple {

class PreferencesManager : public nearby::api::PreferencesManager {
 public:
  explicit PreferencesManager(absl::string_view path);

  // Json
  bool Set(absl::string_view key, const nlohmann::json& value) override;
  nlohmann::json Get(absl::string_view key,
                     const nlohmann::json& default_value) const override;

  // bool
  bool SetBoolean(absl::string_view key, bool value) override;
  bool GetBoolean(absl::string_view key, bool default_value) const override;

  // int
  bool SetInteger(absl::string_view key, int value) override;
  int GetInteger(absl::string_view key, int default_value) const override;

  // int64
  bool SetInt64(absl::string_view key, int64_t value) override;
  int64_t GetInt64(absl::string_view key, int64_t default_value) const override;

  // string
  bool SetString(absl::string_view key, absl::string_view value) override;
  std::string GetString(absl::string_view key,
                        const std::string& default_value) const override;

  // bool array
  bool SetBooleanArray(absl::string_view key,
                       absl::Span<const bool> value) override;
  std::vector<bool> GetBooleanArray(
      absl::string_view key,
      absl::Span<const bool> default_value) const override;

  // int array
  bool SetIntegerArray(absl::string_view key,
                       absl::Span<const int> value) override;
  std::vector<int> GetIntegerArray(
      absl::string_view key,
      absl::Span<const int> default_value) const override;

  // int64 array
  bool SetInt64Array(absl::string_view key,
                     absl::Span<const int64_t> value) override;
  std::vector<int64_t> GetInt64Array(
      absl::string_view key,
      absl::Span<const int64_t> default_value) const override;

  // string array
  bool SetStringArray(absl::string_view key,
                      absl::Span<const std::string> value) override;
  std::vector<std::string> GetStringArray(
      absl::string_view key,
      absl::Span<const std::string> default_value) const override;

  // time
  bool SetTime(absl::string_view key, absl::Time value) override;
  absl::Time GetTime(absl::string_view key,
                     absl::Time default_value) const override;

  // Removes preferences
  void Remove(absl::string_view key) override;
  // TODO: b/485304482 - Implement this method if needed..
  bool RemoveKeyPrefix(absl::string_view prefix) override { return false; }
};

}  // namespace nearby::apple

#endif  // PLATFORM_IMPLEMENTATION_APPLE_PREFERENCES_MANAGER_H_
