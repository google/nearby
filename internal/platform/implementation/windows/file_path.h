// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_FILE_PATH_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_FILE_PATH_H_

#include "internal/base/file_path.h"

namespace nearby::windows {

class FilePath {
 public:
  // If the file already exists we add " (x)", where x is an incrementing
  // number, starting at 1, using the next non-existing number, to the file
  // name, just before the first dot, or at the end if no dot. The absolute path
  // is returned.
  static nearby::FilePath GetCustomSavePath(nearby::FilePath path);
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_FILE_PATH_H_
