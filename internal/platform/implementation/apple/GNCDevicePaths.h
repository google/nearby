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

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

// Returns the path to the LocalAppData directory for the given app path.
NSURL *_Nullable GNCLocalAppDataPath();

// Returns the path to the log directory.
NSURL *_Nullable GNCLogPath();

// Returns the path to the crash dump directory.
NSURL *_Nullable GNCCrashDumpPath();

#ifdef __cplusplus
}  // extern "C"
#endif

NS_ASSUME_NONNULL_END
