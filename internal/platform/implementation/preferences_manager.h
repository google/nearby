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

#ifndef PLATFORM_IMPLEMENTATION_PREFERENCE_MANAGER_H_
#define PLATFORM_IMPLEMENTATION_PREFERENCE_MANAGER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "nlohmann/json_fwd.hpp"

namespace nearby {
namespace api {

// A repository for preferences. The implementations are different on different
// platforms.
//
// The data in preferences has multiple types, such as int, bool, string and
// dictionary. Using proto to describe it is a little complicated. In the
// repository, we use json as the parser for now.
class PreferencesManager {
 public:
  explicit PreferencesManager(absl::string_view path) {}
  virtual ~PreferencesManager() = default;

  // Sets values

  virtual bool Set(absl::string_view key, const nlohmann::json& value) = 0;

  virtual bool SetBoolean(absl::string_view key, bool value) = 0;
  virtual bool SetInteger(absl::string_view key, int value) = 0;
  virtual bool SetInt64(absl::string_view key, int64_t value) = 0;
  virtual bool SetString(absl::string_view key, absl::string_view value) = 0;

  virtual bool SetBooleanArray(absl::string_view key,
                               absl::Span<const bool> value) = 0;
  virtual bool SetIntegerArray(absl::string_view key,
                               absl::Span<const int> value) = 0;
  virtual bool SetInt64Array(absl::string_view key,
                             absl::Span<const int64_t> value) = 0;
  virtual bool SetStringArray(absl::string_view key,
                              absl::Span<const std::string> value) = 0;

  virtual bool SetTime(absl::string_view key, absl::Time value) = 0;

  // Gets values
  virtual nlohmann::json Get(absl::string_view key,
                             const nlohmann::json& default_value) const = 0;

  virtual bool GetBoolean(absl::string_view key, bool default_value) const = 0;
  virtual int GetInteger(absl::string_view key, int default_value) const = 0;
  virtual int64_t GetInt64(absl::string_view key,
                           int64_t default_value) const = 0;
  virtual std::string GetString(absl::string_view key,
                                const std::string& default_value) const = 0;

  virtual std::vector<bool> GetBooleanArray(
      absl::string_view key, absl::Span<const bool> default_value) const = 0;
  virtual std::vector<int> GetIntegerArray(
      absl::string_view key, absl::Span<const int> default_value) const = 0;
  virtual std::vector<int64_t> GetInt64Array(
      absl::string_view key, absl::Span<const int64_t> default_value) const = 0;
  virtual std::vector<std::string> GetStringArray(
      absl::string_view key,
      absl::Span<const std::string> default_value) const = 0;

  virtual absl::Time GetTime(absl::string_view key,
                             absl::Time default_value) const = 0;

  // Removes preferences
  virtual void Remove(absl::string_view key) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_PREFERENCE_MANAGER_H_
