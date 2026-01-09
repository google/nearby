// Copyright 2024 Google LLC
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

#include "internal/platform/implementation/platform.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/output_file.h"

namespace nearby::api {
namespace {

TEST(PlatformTest, CreateOutputFileWithUnixPathSeparator) {
  std::unique_ptr<OutputFile> output_file =
      ImplementationPlatform::CreateOutputFile("C:\\tmp\\path1/path2\\x.txt");
  EXPECT_NE(output_file, nullptr);
  EXPECT_TRUE(output_file->Write("test").Ok());
}

TEST(PlatformTest, GetAppDataPath) {
  std::string app_data_path =
      ImplementationPlatform::GetAppDataPath("test.txt");
  EXPECT_NE(app_data_path, "test.txt");
}

}  // namespace
}  // namespace nearby::api
