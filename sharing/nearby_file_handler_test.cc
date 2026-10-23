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
#include <fstream>
#include <ios>
#include <memory>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/internal/api/mock_sharing_platform.h"

namespace nearby {
namespace sharing {
namespace {
using ::absl::Seconds;
using ::nearby::sharing::api::MockSharingPlatform;

bool CreateFile(FilePath& file_path) {
  std::ofstream file(file_path.GetPath(),
                     std::ios_base::out | std::ios_base::trunc);
  if (!file.good()) {
    return false;
  }
  file.close();
  return true;
}

TEST(NearbyFileHandler, DeleteAFileFromDisk) {
  MockSharingPlatform mock_platform;
  FakeClock clock;
  auto task_runner = std::make_unique<FakeTaskRunner>(&clock, 1);
  FakeTaskRunner* task_runner_ptr = task_runner.get();
  NearbyFileHandler nearby_file_handler(mock_platform, std::move(task_runner));
  FilePath test_file = Files::GetTemporaryDirectory().append(
      FilePath("nearby_nfh_test_abc.jpg"));
  ASSERT_TRUE(CreateFile(test_file));
  std::vector<FilePath> file_paths;
  file_paths.push_back(test_file);
  nearby_file_handler.DeleteFilesFromDisk(file_paths, []() {});
  ASSERT_TRUE(Files::FileExists(test_file));
  EXPECT_TRUE(task_runner_ptr->SyncWithTimeout(Seconds(5)));
  ASSERT_FALSE(Files::FileExists(test_file));
}

TEST(NearbyFileHandler, DeleteMultipleFilesFromDisk) {
  MockSharingPlatform mock_platform;
  FakeClock clock;
  auto task_runner = std::make_unique<FakeTaskRunner>(&clock, 1);
  FakeTaskRunner* task_runner_ptr = task_runner.get();
  NearbyFileHandler nearby_file_handler(mock_platform, std::move(task_runner));
  FilePath test_file = Files::GetTemporaryDirectory().append(
      FilePath("nearby_nfh_test_abc.jpg"));
  FilePath test_file2 = Files::GetTemporaryDirectory().append(
      FilePath("nearby_nfh_test_def.jpg"));
  FilePath test_file3 = Files::GetTemporaryDirectory().append(
      FilePath("nearby_nfh_test_ghi.jpg"));
  std::vector<FilePath> file_paths;
  file_paths = {test_file, test_file2, test_file3};
  // Check it doesn't throw an exception.
  nearby_file_handler.DeleteFilesFromDisk(file_paths, []() {});
  ASSERT_FALSE(Files::FileExists(test_file));
  ASSERT_FALSE(Files::FileExists(test_file2));
  ASSERT_FALSE(Files::FileExists(test_file3));

  EXPECT_TRUE(task_runner_ptr->SyncWithTimeout(Seconds(5)));
  ASSERT_FALSE(Files::FileExists(test_file));
  ASSERT_FALSE(Files::FileExists(test_file2));
  ASSERT_FALSE(Files::FileExists(test_file3));
}

TEST(NearbyFileHandler, TestCallback) {
  MockSharingPlatform mock_platform;
  std::atomic_bool received_callback = false;
  FakeClock clock;
  auto task_runner = std::make_unique<FakeTaskRunner>(&clock, 1);
  FakeTaskRunner* task_runner_ptr = task_runner.get();
  NearbyFileHandler nearby_file_handler(mock_platform, std::move(task_runner));
  FilePath test_file = Files::GetTemporaryDirectory().append(
      FilePath("nearby_nfh_test_abc.jpg"));
  ASSERT_TRUE(CreateFile(test_file));
  std::vector<FilePath> file_paths;
  file_paths.push_back(test_file);
  nearby_file_handler.DeleteFilesFromDisk(
      file_paths, [&received_callback]() { received_callback = true; });
  ASSERT_FALSE(received_callback);
  ASSERT_TRUE(Files::FileExists(test_file));
  EXPECT_TRUE(task_runner_ptr->SyncWithTimeout(Seconds(5)));
  ASSERT_TRUE(received_callback);
  ASSERT_FALSE(Files::FileExists(test_file));
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
