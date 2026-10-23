// Copyright 2020 Google LLC
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

#import "internal/platform/implementation/apple/GNCDevicePaths.h"

#import "internal/platform/implementation/apple/Log/GNCLogger.h"

NS_ASSUME_NONNULL_BEGIN

NSURL *_Nullable GNCLocalAppDataPath() {
  NSFileManager *manager = [NSFileManager defaultManager];

  NSError *error = nil;
  NSURL *applicationSupportURL = [manager URLForDirectory:NSApplicationSupportDirectory
                                                 inDomain:NSUserDomainMask
                                        appropriateForURL:nil
                                                   create:YES
                                                    error:&error];
  if (!applicationSupportURL) {
    GNCLoggerError(@"Failed to get application support path: %@", error);
    return nil;
  }

  return applicationSupportURL;
}

NSURL *_Nullable GNCLogPath() {
  return [GNCLocalAppDataPath() URLByAppendingPathComponent:@"Google/Nearby/Sharing/Logs"];
}

NSURL *_Nullable GNCCrashDumpPath() {
  return [GNCLocalAppDataPath() URLByAppendingPathComponent:@"Google/Nearby/Sharing/CrashDumps"];
}

NS_ASSUME_NONNULL_END
