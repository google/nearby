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

#ifndef PLATFORM_IMPLEMENTATION_WINDOWS_PREFERENCES_REPOSITORY_H_
#define PLATFORM_IMPLEMENTATION_WINDOWS_PREFERENCES_REPOSITORY_H_

#include <optional>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "internal/base/file_path.h"

namespace nearby {
namespace windows {

class PreferencesRepository {
 public:
  explicit PreferencesRepository(const FilePath& path) : path_(path) {}

  nlohmann::json LoadPreferences() ABSL_LOCKS_EXCLUDED(&mutex_);
  bool SavePreferences(nlohmann::json preferences) ABSL_LOCKS_EXCLUDED(&mutex_);

  std::optional<nlohmann::json> AttemptLoad();
  std::optional<nlohmann::json> RestoreFromBackup();

 private:
  absl::Mutex mutex_;
  const FilePath path_;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPLEMENTATION_WINDOWS_PREFERENCES_REPOSITORY_H_
