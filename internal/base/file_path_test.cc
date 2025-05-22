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

#include "internal/base/file_path.h"

#include "gtest/gtest.h"

namespace nearby {
namespace {

TEST(FilePathTest, FromUTF8Windows) {
  FilePath path("C:\\Users\\奥巴马\\Documents");
  EXPECT_EQ(path.ToString(), "C:\\Users\\奥巴马\\Documents");
}

TEST(FilePathTest, FromUTF8Linux) {
  FilePath path("/usr/local/home/奥巴马/Documents");
  EXPECT_EQ(path.ToString(), "/usr/local/home/奥巴马/Documents");
}

TEST(FilePathTest, FromAscii) {
  FilePath path("/usr/local/home/bob/Documents");
  EXPECT_EQ(path.ToString(), "/usr/local/home/bob/Documents");
}

TEST(FilePathTest, FromUnicodeWindows) {
  FilePath path(L"C:\\Users\\奥巴马\\Documents");
  EXPECT_EQ(path.ToString(), "C:\\Users\\奥巴马\\Documents");
}

TEST(FilePathTest, FromUnicodeLinux) {
  FilePath path(L"/usr/local/home/奥巴马/Documents");
  EXPECT_EQ(path.ToString(), "/usr/local/home/奥巴马/Documents");
}

TEST(FilePathTest, FromUTF8WindowsToWideString) {
  FilePath path("C:\\Users\\奥巴马\\Documents");
  EXPECT_EQ(path.ToWideString(), L"C:\\Users\\奥巴马\\Documents");
}

TEST(FilePathTest, FromUTF8LinuxToWideString) {
  FilePath path("/usr/local/home/奥巴马/Documents");
  EXPECT_EQ(path.ToWideString(), L"/usr/local/home/奥巴马/Documents");
}

TEST(FilePathTest, FromAsciiToWideString) {
  FilePath path("/usr/local/home/bob/Documents");
  EXPECT_EQ(path.ToWideString(), L"/usr/local/home/bob/Documents");
}

TEST(FilePathTest, FromUnicodeWindowsToWideString) {
  FilePath path(L"C:\\Users\\奥巴马\\Documents");
  EXPECT_EQ(path.ToWideString(), L"C:\\Users\\奥巴马\\Documents");
}

TEST(FilePathTest, FromUnicodeLinuxToWideString) {
  FilePath path(L"/usr/local/home/奥巴马/Documents");
  EXPECT_EQ(path.ToWideString(), L"/usr/local/home/奥巴马/Documents");
}

TEST(FilePathTest, IsEmptySuccess) {
  FilePath path;
  EXPECT_TRUE(path.IsEmpty());
  FilePath path2("/usr/local/home/奥巴马/Documents/test.pdf");
  EXPECT_FALSE(path2.IsEmpty());
}

TEST(FilePathTest, GetExtensionEmpty) {
  FilePath path("/usr/local/home/test/Documents/test.");
  EXPECT_EQ(path.GetExtension().ToString(), ".");
}

TEST(FilePathTest, GetExtensionSuccess) {
  FilePath path("/usr/local/home/test/Documents/test.奥巴马");
  EXPECT_EQ(path.GetExtension().ToString(), ".奥巴马");
}
TEST(FilePathTest, GetFileNameSuccess) {
  FilePath path("/usr/local/home/test/奥巴马.pdf");
  EXPECT_EQ(path.GetFileName().ToString(), "奥巴马.pdf");
}

TEST(FilePathTest, AppendSuccess) {
  FilePath path("/usr/local/home/奥巴马/Documents");
  FilePath sub_path("贝拉克/temp");

  path.append(sub_path);
#if defined(_WIN32)
  EXPECT_EQ(path.ToWideString(),
            L"/usr/local/home/奥巴马/Documents\\贝拉克/temp");
#else
  EXPECT_EQ(path.ToWideString(),
            L"/usr/local/home/奥巴马/Documents/贝拉克/temp");
#endif
}

TEST(FilePathTest, GetParentPathSuccess) {
  FilePath path("/usr/local/home/奥巴马/Documents");
  EXPECT_EQ(path.GetParentPath().ToString(), "/usr/local/home/奥巴马");
}

}  // namespace
}  // namespace nearby
