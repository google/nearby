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

#include "internal/base/files.h"

#include <cstdint>
#include <filesystem>  // NOLINT
#include <fstream>
#include <ios>
#include <optional>

#include "gtest/gtest.h"

namespace nearby::sharing {
namespace {

TEST(FilesTest, CreateHardLinkSuccess) {
  std::filesystem::path temp_dir = testing::TempDir();
  std::filesystem::path target = temp_dir / "target";
  RemoveFile(target);
  std::ofstream ofstream(target, std::ios::app);
  ASSERT_EQ(ofstream.rdstate(), std::ios_base::goodbit);
  ofstream << "Hello world";
  ofstream.flush();
  std::optional<uintmax_t> size = GetFileSize(target);
  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(size.value(), 11);
  std::filesystem::path link_path = temp_dir / "link_path";
  EXPECT_TRUE(CreateHardLink(target, link_path));
  EXPECT_TRUE(FileExists(link_path));
  EXPECT_EQ(GetFileSize(link_path), 11);
  RemoveFile(link_path);
  RemoveFile(target);
}

}  // namespace
}  // namespace nearby::sharing
