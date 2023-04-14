// Copyright 2021 Google LLC
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

#include "internal/platform/implementation/g3/preferences_repository.h"

#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>

#include "absl/synchronization/mutex.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

namespace nearby {
namespace g3 {
namespace {
using json = nlohmann::json;
}  // namespace

json PreferencesRepository::LoadPreferences() {
  absl::MutexLock lock(&mutex_);

  // Emulate Windows implementation
  try {
    // settings.json is used for testing, but we should look at having
    // an implementation override for G3 in PreferencesManager to override
    // the path for testing.
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "settings.json";
    if (!std::filesystem::exists(path)) {
      return value_;
    }

    std::ifstream preferences_file(path.c_str());
    if (!preferences_file.good()) {
      return value_;
    }
    json preferences = json::parse(preferences_file, nullptr, false);
    preferences_file.close();

    if (preferences.is_discarded()) {
      return value_;
    }

    value_ = preferences;
  } catch (...) {
    return value_;
  }

  return value_;
}

bool PreferencesRepository::SavePreferences(json preferences) {
  absl::MutexLock lock(&mutex_);
  value_ = preferences;
  return true;
}

}  // namespace g3
}  // namespace nearby
