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

#include "sharing/internal/test/fake_shell.h"

#include <filesystem>  // NOLINT(build/c++17)
#include <string>

#include "gtest/gtest.h"
#include "absl/status/status.h"

namespace nearby {
namespace {

TEST(FakeShell, Open) {
  FakeShell shell;
  absl::Status result;
  shell.Open(std::filesystem::temp_directory_path(),
             [&](absl::Status status) { result = status; });
  EXPECT_TRUE(result.ok());
}

TEST(FakeShell, OpenFailed) {
  FakeShell shell;
  shell.set_return_error(true);
  absl::Status result;
  shell.Open("c:\\windows", [&](absl::Status status) { result = status; });
}

}  // namespace
}  // namespace nearby
