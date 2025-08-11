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

#include "internal/platform/implementation/windows/device_paths.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/base/file_path.h"

namespace nearby::platform::windows {
namespace {

using ::testing::Eq;

TEST(Win32ProcessTest, GetLocalAppDataPath) {
  FilePath path = GetLocalAppDataPath(FilePath("test\\data"));
  EXPECT_THAT(path,
              Eq(FilePath("C:\\Users\\lexan\\AppData\\Local\\test\\data")));
}

TEST(Win32ProcessTest, GetLogPath) {
  FilePath path = GetLogPath();
  EXPECT_THAT(
      path,
      Eq(FilePath(
          "C:\\Users\\lexan\\AppData\\Local\\Google\\Nearby\\Sharing\\Logs")));
}

TEST(Win32ProcessTest, GetCrashDumpPath) {
  FilePath path = GetCrashDumpPath();
  EXPECT_THAT(path, Eq(FilePath("C:"
                                "\\Users\\lexan\\AppData\\Local\\Google\\Nearby"
                                "\\Sharing\\CrashDumps")));
}
}  // namespace
}  // namespace nearby::platform::windows
