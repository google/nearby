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

#ifndef PLATFORM_IMPLEMENTATION_G3_PREFERENCES_REPOSITORY_H_
#define PLATFORM_IMPLEMENTATION_G3_PREFERENCES_REPOSITORY_H_

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"
#include "internal/base/file_path.h"

namespace nearby::g3 {

class PreferencesRepository {
 public:
  explicit PreferencesRepository(FilePath base_path)
      : file_path_(base_path.append(FilePath("settings.json"))) {}

  nlohmann::json LoadPreferences() ABSL_LOCKS_EXCLUDED(&mutex_);
  bool SavePreferences(nlohmann::json preferences) ABSL_LOCKS_EXCLUDED(&mutex_);

 private:
  const FilePath file_path_;
  // Avoid to write in google3, just create a memory value to simulate a
  // preferences storage
  nlohmann::json value_ = nlohmann::json::object();
  absl::Mutex mutex_;
};

}  // namespace nearby::g3

#endif  // PLATFORM_IMPLEMENTATION_G3_PREFERENCES_REPOSITORY_H_
