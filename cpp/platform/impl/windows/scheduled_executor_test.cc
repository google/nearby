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
#include "platform/impl/windows/scheduled_executor.h"

#include <utility>

#include "platform/impl/windows/test_data.h"

#include "gtest/gtest.h"

TEST(ScheduledExecutorTests, ExecuteSucceeds) {
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  std::unique_ptr<location::nearby::windows::ScheduledExecutor>
      submittableExecutor =
          std::make_unique<location::nearby::windows::ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  submittableExecutor->Execute([&output, &threadIds]() {
    threadIds->push_back(GetCurrentThreadId());
    output.append(RUNNABLE_0_TEXT.c_str());
  });

  submittableExecutor->Shutdown();

  //  Assert
  //  We should've run 1 time on the main thread, and 1 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(ScheduledExecutorTests, ScheduleSucceeds) {
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  std::unique_ptr<location::nearby::windows::ScheduledExecutor>
      submittableExecutor =
          std::make_unique<location::nearby::windows::ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  std::chrono::system_clock::time_point timeNow =
      std::chrono::system_clock::now();
  std::chrono::system_clock::time_point timeExecuted;

  // Act
  submittableExecutor->Schedule(
      [&output, &threadIds, &timeExecuted]() {
        timeExecuted = std::chrono::system_clock::now();
        threadIds->push_back(GetCurrentThreadId());
        output.append(RUNNABLE_0_TEXT.c_str());
      },
      absl::Milliseconds(50));

  SleepEx(100, true);  //  Yield the thread

  submittableExecutor->Shutdown();

  auto difference = std::chrono::duration_cast<std::chrono::milliseconds>(
                        timeExecuted - timeNow)
                        .count();

  //  Assert
  //  We should've run 1 time on the main thread, and 1 times on the
  //  workerThread
  ASSERT_TRUE(difference >= 50) << "difference was: " << difference;
  ASSERT_TRUE(difference < 100) << "difference was: " << difference;

  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(ScheduledExecutorTests, CancelSucceeds) {
  // Arrange
  std::string expected("");

  std::unique_ptr<location::nearby::windows::ScheduledExecutor>
      submittableExecutor =
          std::make_unique<location::nearby::windows::ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  auto cancelable = submittableExecutor->Schedule(
      [&output, &threadIds]() {
        threadIds->push_back(GetCurrentThreadId());
        output.append(RUNNABLE_0_TEXT.c_str());
      },
      absl::Milliseconds(1000));

  SleepEx(100, true);  //  Yield the thread

  auto actual = cancelable->Cancel();

  submittableExecutor->Shutdown();

  // Assert
  ASSERT_TRUE(actual);
  ASSERT_EQ(threadIds->size(), 1);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(ScheduledExecutorTests, CancelAfterStartedFails) {
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  std::unique_ptr<location::nearby::windows::ScheduledExecutor>
      submittableExecutor =
          std::make_unique<location::nearby::windows::ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  auto cancelable = submittableExecutor->Schedule(
      [&output, &threadIds]() {
        threadIds->push_back(GetCurrentThreadId());
        output.append(RUNNABLE_0_TEXT.c_str());
      },
      absl::Milliseconds(100));

  SleepEx(1000, true);  //  Yield the thread

  auto actual = cancelable->Cancel();

  submittableExecutor->Shutdown();

  // Assert
  ASSERT_FALSE(actual);
  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}
