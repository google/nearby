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

#ifndef PLATFORM_IMPLEMENTATION_PREFERENCE_REPOSITORY_H_
#define PLATFORM_IMPLEMENTATION_PREFERENCE_REPOSITORY_H_

#include <string>

#include "absl/strings/string_view.h"
#include "nlohmann/json_fwd.hpp"

namespace nearby {
namespace api {

// A repository for preferences. The implementations are different on different
// platforms.
//
// The data in preferences has multiple types, such as int, bool, string and
// dictionary. Using proto to describe it is a little complicated. In the
// repository, we use json as the parser for now.
class PreferencesRepository {
 public:
  explicit PreferencesRepository(absl::string_view path) : path_(path) {}
  virtual ~PreferencesRepository() = default;

  virtual nlohmann::json LoadPreferences() = 0;
  virtual bool SavePreferences(nlohmann::json preferences) = 0;

 protected:
  std::string path_;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_PREFERENCE_REPOSITORY_H_
