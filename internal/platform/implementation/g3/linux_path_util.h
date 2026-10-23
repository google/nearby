// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_G3_LINUX_PATH_UTIL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_G3_LINUX_PATH_UTIL_H_

#include <cstdlib>

#include "internal/base/file_path.h"
#include "internal/base/files.h"

namespace nearby::g3 {

inline FilePath GetAppDataPath() {
  const char* home_dir = getenv("HOME");
  if (home_dir == nullptr) {
    return Files::GetTemporaryDirectory();
  }
  // This matches the .NET LocalAppData directory on Linux.
  return FilePath(home_dir).append(FilePath(".local/share"));
}

}  // namespace nearby::g3

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_G3_LINUX_PATH_UTIL_H_
