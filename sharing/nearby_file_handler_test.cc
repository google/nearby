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

#include "sharing/nearby_file_handler.h"

#include <cstdio>
#include <filesystem>  // NOLINT(build/c++17)
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace nearby {
namespace sharing {
namespace {

bool CreateFile(std::filesystem::path file_path) {
  std::FILE* file = std::fopen(file_path.string().c_str(), "w+");
  if (file == nullptr) {
    return false;
  }
  std::fclose(file);
  return true;
}

bool DeleteFile(std::filesystem::path file_path) {
  if (std::filesystem::exists(file_path)) {
    return std::filesystem::remove(file_path);
  }

  return true;
}

bool ExistFile(std::filesystem::path file_path) {
  if (std::filesystem::exists(file_path)) {
    return true;
  }

  return false;
}

TEST(NearbyFileHandler, GetUniquePath) {
  NearbyFileHandler nearby_file_handler;
  std::filesystem::path unique_path;
  absl::Notification notification;

  std::filesystem::path test_file =
      std::filesystem::temp_directory_path() / "nearby_nfh_test_abc.jpg";
  std::filesystem::path expected_file =
      std::filesystem::temp_directory_path() / "nearby_nfh_test_abc.jpg";

  ASSERT_TRUE(CreateFile(test_file));
  ASSERT_TRUE(DeleteFile(expected_file));

  nearby_file_handler.GetUniquePath(
      test_file, [&notification, &unique_path](std::filesystem::path path) {
        unique_path = path;
        notification.Notify();
      });

  notification.WaitForNotificationWithTimeout(absl::Seconds(1));
  EXPECT_EQ(unique_path, expected_file);
}

TEST(NearbyFileHandler, OpenFiles) {
  NearbyFileHandler nearby_file_handler;
  absl::Notification notification;
  std::vector<NearbyFileHandler::FileInfo> result;
  std::filesystem::path test_file =
      std::filesystem::temp_directory_path() / "nearby_nfh_test_abc.jpg";

  ASSERT_TRUE(CreateFile(test_file));
  nearby_file_handler.OpenFiles(
      {test_file}, [&result, &notification](
                       std::vector<NearbyFileHandler::FileInfo> file_infos) {
        result = file_infos;
        notification.Notify();
      });

  notification.WaitForNotificationWithTimeout(absl::Seconds(1));
  EXPECT_EQ(result.size(), 1);
  ASSERT_TRUE(DeleteFile(test_file));
}

TEST(NearbyFileHandler, DeleteAFileFromDisk) {
  NearbyFileHandler nearby_file_handler;
  std::filesystem::path test_file =
      std::filesystem::temp_directory_path() / "nearby_nfh_test_abc.jpg";
  ASSERT_TRUE(CreateFile(test_file));
  std::vector<std::filesystem::path> file_paths;
  file_paths.push_back(test_file);
  nearby_file_handler.DeleteFilesFromDisk(file_paths, []() {});
  ASSERT_TRUE(ExistFile(test_file));
  absl::SleepFor(absl::Seconds(2));
  ASSERT_FALSE(ExistFile(test_file));
}

TEST(NearbyFileHandler, DeleteMultipleFilesFromDisk) {
  NearbyFileHandler nearby_file_handler;
  std::filesystem::path test_file =
      std::filesystem::temp_directory_path() / "nearby_nfh_test_abc.jpg";
  std::filesystem::path test_file2 =
      std::filesystem::temp_directory_path() / "nearby_nfh_test_def.jpg";
  std::filesystem::path test_file3 =
      std::filesystem::temp_directory_path() / "nearby_nfh_test_ghi.jpg";
  std::vector<std::filesystem::path> file_paths;
  file_paths = {test_file, test_file2, test_file3};
  // Check it doesn't throw an exception.
  nearby_file_handler.DeleteFilesFromDisk(file_paths, []() {});
  ASSERT_FALSE(ExistFile(test_file));
  ASSERT_FALSE(ExistFile(test_file2));
  ASSERT_FALSE(ExistFile(test_file3));
  absl::SleepFor(absl::Seconds(2));
  ASSERT_FALSE(ExistFile(test_file));
  ASSERT_FALSE(ExistFile(test_file2));
  ASSERT_FALSE(ExistFile(test_file3));
}

TEST(NearbyFileHandler, TestCallback) {
  bool received_callback = false;
  NearbyFileHandler nearby_file_handler;
  std::filesystem::path test_file =
      std::filesystem::temp_directory_path() / "nearby_nfh_test_abc.jpg";
  ASSERT_TRUE(CreateFile(test_file));
  std::vector<std::filesystem::path> file_paths;
  file_paths.push_back(test_file);
  nearby_file_handler.DeleteFilesFromDisk(
      file_paths, [&received_callback]() { received_callback = true; });
  ASSERT_FALSE(received_callback);
  ASSERT_TRUE(ExistFile(test_file));
  absl::SleepFor(absl::Seconds(2));
  ASSERT_TRUE(received_callback);
  ASSERT_FALSE(ExistFile(test_file));
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
