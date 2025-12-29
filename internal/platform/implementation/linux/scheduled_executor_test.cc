// Copyright 2021 Google LLC
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

#include <memory>
#include <utility>

#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "internal/platform/implementation/linux/test_data.h"
#include "internal/platform/implementation/linux/scheduled_executor.h"

namespace nearby {
namespace linux {
namespace {

TEST(ScheduledExecutorTests, ExecuteSucceeds) {
  absl::Notification notification;
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  auto submittableExecutor = std::make_unique<ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<std::thread::id>> threadIds =
      std::make_unique<std::vector<std::thread::id>>();

  threadIds->push_back(std::this_thread::get_id());

  // Act
  submittableExecutor->Execute([&]() {
    threadIds->push_back(std::this_thread::get_id());
    output.append(RUNNABLE_0_TEXT.c_str());
    notification.Notify();
  });

  ASSERT_TRUE(
      notification.WaitForNotificationWithTimeout(absl::Milliseconds(200)));
  submittableExecutor->Shutdown();

  //  Assert
  //  We should've run 1 time on the main thread, and 1 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(std::this_thread::get_id(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(ScheduledExecutorTests, ScheduleSucceeds) {
  absl::Notification notification;
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  auto submittableExecutor = std::make_unique<ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<std::thread::id>> threadIds =
      std::make_unique<std::vector<std::thread::id>>();

  threadIds->push_back(std::this_thread::get_id());

  std::chrono::system_clock::time_point timeNow =
      std::chrono::system_clock::now();
  std::chrono::system_clock::time_point timeExecuted;

  // Act
  submittableExecutor->Schedule(
      [&]() {
        timeExecuted = std::chrono::system_clock::now();
        threadIds->push_back(std::this_thread::get_id());
        output.append(RUNNABLE_0_TEXT.c_str());
        notification.Notify();
      },
      absl::Milliseconds(50));

  ASSERT_TRUE(
      notification.WaitForNotificationWithTimeout(absl::Milliseconds(200)));
  submittableExecutor->Shutdown();

  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(std::this_thread::get_id(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(ScheduledExecutorTests, CancelSucceeds) {
  absl::Notification notification;
  // Arrange
  std::string expected("");

  auto submittableExecutor = std::make_unique<ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<std::thread::id>> threadIds =
      std::make_unique<std::vector<std::thread::id>>();

  threadIds->push_back(std::this_thread::get_id());

  // Act
  auto cancelable = submittableExecutor->Schedule(
      [&]() {
        threadIds->push_back(std::this_thread::get_id());
        output.append(RUNNABLE_0_TEXT.c_str());
        notification.Notify();
      },
      absl::Milliseconds(1000));

  auto actual = cancelable->Cancel();

  EXPECT_FALSE(
      notification.WaitForNotificationWithTimeout(absl::Milliseconds(2000)));
  submittableExecutor->Shutdown();

  // Assert
  ASSERT_TRUE(actual);
  ASSERT_EQ(threadIds->size(), 1);
  //  We should still be on the main thread
  ASSERT_EQ(std::this_thread::get_id(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(ScheduledExecutorTests, CancelAfterStartedFails) {
  absl::Notification notification;
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  auto submittableExecutor = std::make_unique<ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<std::thread::id>> threadIds =
      std::make_unique<std::vector<std::thread::id>>();

  threadIds->push_back(std::this_thread::get_id());

  // Act
  auto cancelable = submittableExecutor->Schedule(
      [&]() {
        threadIds->push_back(std::this_thread::get_id());
        output.append(RUNNABLE_0_TEXT.c_str());
        notification.Notify();
      },
      absl::Milliseconds(100));

  absl::SleepFor(absl::Milliseconds(200));
  auto actual = cancelable->Cancel();

  ASSERT_TRUE(
      notification.WaitForNotificationWithTimeout(absl::Milliseconds(2000)));
  submittableExecutor->Shutdown();

  // Assert
  ASSERT_FALSE(actual);
  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(std::this_thread::get_id(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

}  // namespace
}  // namespace linux
}  // namespace nearby
