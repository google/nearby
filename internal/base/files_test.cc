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
#include <fstream>
#include <ios>
#include <optional>

#include "gtest/gtest.h"
#include "internal/base/file_path.h"

namespace nearby {
namespace {

TEST(FilesTest, CreateHardLinkSuccess) {
  FilePath temp_dir{testing::TempDir()};
  FilePath target = temp_dir;
  target.append(FilePath("target"));
  Files::RemoveFile(target);
  std::ofstream ofstream(target.GetPath(), std::ios::app);
  ASSERT_EQ(ofstream.rdstate(), std::ios_base::goodbit);
  ofstream << "Hello world";
  ofstream.flush();
  std::optional<uintmax_t> size = Files::GetFileSize(target);
  ASSERT_TRUE(size.has_value());
  EXPECT_EQ(size.value(), 11);
  FilePath link_path = temp_dir;
  link_path.append(FilePath("link_path"));
  EXPECT_TRUE(Files::CreateHardLink(target, link_path));
  EXPECT_TRUE(Files::FileExists(link_path));
  EXPECT_EQ(Files::GetFileSize(link_path), 11);
  Files::RemoveFile(link_path);
  Files::RemoveFile(target);
}

}  // namespace
}  // namespace nearby
