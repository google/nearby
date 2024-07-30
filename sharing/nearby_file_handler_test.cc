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

#include <atomic>
#include <cstdio>
#include <filesystem>  // NOLINT(build/c++17)
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/base/files.h"
#include "sharing/internal/api/mock_sharing_platform.h"

namespace nearby {
namespace sharing {
namespace {
using ::nearby::sharing::api::MockSharingPlatform;

bool CreateFile(std::filesystem::path file_path) {
  std::FILE* file = std::fopen(file_path.string().c_str(), "w+");
  if (file == nullptr) {
    return false;
  }
  std::fclose(file);
  return true;
}

TEST(NearbyFileHandler, OpenFiles) {
  MockSharingPlatform mock_platform;
  NearbyFileHandler nearby_file_handler(mock_platform);
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
  ASSERT_TRUE(RemoveFile(test_file));
}

TEST(NearbyFileHandler, DeleteAFileFromDisk) {
  MockSharingPlatform mock_platform;
  NearbyFileHandler nearby_file_handler(mock_platform);
  std::filesystem::path test_file =
      std::filesystem::temp_directory_path() / "nearby_nfh_test_abc.jpg";
  ASSERT_TRUE(CreateFile(test_file));
  std::vector<std::filesystem::path> file_paths;
  file_paths.push_back(test_file);
  nearby_file_handler.DeleteFilesFromDisk(file_paths, []() {});
  ASSERT_TRUE(FileExists(test_file));
  absl::SleepFor(absl::Seconds(2));
  ASSERT_FALSE(FileExists(test_file));
}

TEST(NearbyFileHandler, DeleteMultipleFilesFromDisk) {
  MockSharingPlatform mock_platform;
  NearbyFileHandler nearby_file_handler(mock_platform);
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
  ASSERT_FALSE(FileExists(test_file));
  ASSERT_FALSE(FileExists(test_file2));
  ASSERT_FALSE(FileExists(test_file3));
  absl::SleepFor(absl::Seconds(2));
  ASSERT_FALSE(FileExists(test_file));
  ASSERT_FALSE(FileExists(test_file2));
  ASSERT_FALSE(FileExists(test_file3));
}

TEST(NearbyFileHandler, TestCallback) {
  MockSharingPlatform mock_platform;
  std::atomic_bool received_callback = false;
  NearbyFileHandler nearby_file_handler(mock_platform);
  std::filesystem::path test_file =
      std::filesystem::temp_directory_path() / "nearby_nfh_test_abc.jpg";
  ASSERT_TRUE(CreateFile(test_file));
  std::vector<std::filesystem::path> file_paths;
  file_paths.push_back(test_file);
  nearby_file_handler.DeleteFilesFromDisk(
      file_paths, [&received_callback]() { received_callback = true; });
  ASSERT_FALSE(received_callback);
  ASSERT_TRUE(FileExists(test_file));
  absl::SleepFor(absl::Seconds(2));
  ASSERT_TRUE(received_callback);
  ASSERT_FALSE(FileExists(test_file));
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
