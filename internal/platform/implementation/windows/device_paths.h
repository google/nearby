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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_DEVICE_PATHS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_DEVICE_PATHS_H_

#include "internal/base/file_path.h"

namespace nearby::platform::windows {
// Utility functions for getting the paths of various device directories.

// Returns the path to the LocalAppData directory for the given app path.
FilePath GetLocalAppDataPath(const FilePath& app_path);

// Returns the path to the log directory.
FilePath GetLogPath();

// Returns the path to the crash dump directory.
FilePath GetCrashDumpPath();

}  // namespace nearby::platform::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_DEVICE_PATHS_H_
