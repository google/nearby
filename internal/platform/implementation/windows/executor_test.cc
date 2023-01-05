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

#include "internal/platform/implementation/windows/executor.h"

#include <algorithm>
#include <utility>

#include "gtest/gtest.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/windows/test_data.h"

namespace nearby {
namespace windows {
namespace {

constexpr absl::Duration kWaitTimeout = absl::Milliseconds(200);

TEST(ExecutorTests, SingleThreadedExecutorSucceeds) {
  absl::Notification notification;
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  auto executor = std::make_unique<Executor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  executor->Execute([&]() {
    threadIds->push_back(GetCurrentThreadId());
    output.append(RUNNABLE_0_TEXT.c_str());
    notification.Notify();
  });

  ASSERT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  executor->Shutdown();

  //  Assert
  //  We should've run 1 time on the main thread, and 5 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(ExecutorTests, SingleThreadedExecutorAfterShutdownFails) {
  // Arrange
  std::string expected("");

  std::unique_ptr<Executor> executor = std::make_unique<Executor>();
  std::unique_ptr<std::string> output = std::make_unique<std::string>();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());
  executor->Shutdown();

  // Act
  executor->Execute([&output, &threadIds]() {
    threadIds->push_back(GetCurrentThreadId());
    output->append(RUNNABLE_0_TEXT.c_str());
  });

  //  Assert
  //  We should've run 1 time on the main thread, and 5 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 1);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(*output.get(), expected);
}

TEST(ExecutorTests, SingleThreadedExecutorExecuteNullSucceeds) {
  absl::Notification notification;
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  auto executor = std::make_unique<Executor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  executor->Execute(nullptr);
  executor->Execute([&]() {
    threadIds->push_back(GetCurrentThreadId());
    output.append(RUNNABLE_0_TEXT.c_str());
    notification.Notify();
  });
  executor->Execute(nullptr);

  ASSERT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  executor->Shutdown();

  //  Assert
  //  We should've run 1 time on the main thread, and 5 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(ExecutorTests, SingleThreadedExecutorMultipleTasksSucceeds) {
  absl::BlockingCounter block_count(5);

  // Arrange
  std::string expected(RUNNABLE_ALL_TEXT.c_str());

  auto executor = std::make_unique<Executor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  auto parent_thread = GetCurrentThreadId();

  // Act
  for (int index = 0; index < 5; index++) {
    executor->Execute([&, index]() {
      threadIds->push_back(GetCurrentThreadId());
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "%s%d, ", RUNNABLE_TEXT.c_str(), index);
      output.append(std::string(buffer));
      block_count.DecrementCount();
    });
  }

  block_count.Wait();
  executor->Shutdown();

  //  Assert
  //  We should've run 1 time on the main thread, and 5 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 5);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), parent_thread);
  //  We should've run all runnables on the worker thread
  auto workerThreadId = threadIds->at(0);
  for (int index = 0; index < threadIds->size(); index++) {
    ASSERT_EQ(threadIds->at(index), workerThreadId);
  }

  //  We should of run them in the order submitted
  ASSERT_EQ(output, expected);
}

TEST(ExecutorTests, MultiThreadedExecutorSingleTaskSucceeds) {
  absl::Notification notification;

  //  Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  auto executor = std::make_unique<Executor>(2);

  //  Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  std::shared_ptr<std::string> output = std::make_shared<std::string>();

  threadIds->push_back(GetCurrentThreadId());

  //  Act
  executor->Execute([&, output]() {
    threadIds->push_back(GetCurrentThreadId());
    output->append(RUNNABLE_0_TEXT.c_str());
    notification.Notify();
  });

  ASSERT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  executor->Shutdown();

  //  Assert
  //  We should've run 1 time on the main thread, and 5 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run the task
  ASSERT_EQ(*output.get(), expected);
}

TEST(ExecutorTests, MultiThreadedExecutorMultipleTasksSucceeds) {
  absl::BlockingCounter block_count(5);

  //  Arrange
  auto executor = std::make_unique<Executor>(2);

  //  Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  std::shared_ptr<std::string> output = std::make_shared<std::string>();

  threadIds->push_back(GetCurrentThreadId());

  //  Act
  for (int index = 0; index < 5; index++) {
    executor->Execute([&, index]() {
      threadIds->push_back(GetCurrentThreadId());
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "%s %d, ", RUNNABLE_TEXT.c_str(), index);
      output->append(std::string(buffer));
      block_count.DecrementCount();
    });
  }

  block_count.Wait();
  executor->Shutdown();

  //  Assert
  //  We should've run 1 time on the main thread, and 5 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 6);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
}

TEST(ExecutorTests, MultiThreadedExecutorSingleTaskAfterShutdownFails) {
  //  Arrange
  std::string expected("");

  auto executor = std::make_unique<Executor>(2);

  //  Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  std::shared_ptr<std::string> output = std::make_shared<std::string>();

  threadIds->push_back(GetCurrentThreadId());

  executor->Shutdown();

  //  Act
  executor->Execute([output, &threadIds]() {
    threadIds->push_back(GetCurrentThreadId());
    output->append(RUNNABLE_0_TEXT.c_str());
  });

  //  Assert
  //  We should've run 1 time on the main thread, and 5 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 1);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run the task
  ASSERT_EQ(*output.get(), expected);
}

TEST(ExecutorTests,
     MultiThreadedExecutorMultipleTasksLargeNumberOfThreadsSucceeds) {
  absl::BlockingCounter block_count(250);

  //  Arrange
  auto executor = std::make_unique<Executor>(32);

  //  Container to note threads that ran
  std::vector<DWORD> threadIds = std::vector<DWORD>();

  threadIds.push_back(GetCurrentThreadId());
  absl::Mutex mutex;
  //  Act
  for (int index = 0; index < 250; index++) {
    executor->Execute([&]() mutable {
      DWORD id = GetCurrentThreadId();
      {
        absl::MutexLock lock(&mutex);
        threadIds.push_back(id);
      }

      // Using rand since this is in a critical section
      // and windows doesn't have a rand_r anyway
      auto sleepTime = (std::rand() % 101) + 1;  //  NOLINT

      Sleep(sleepTime);
      block_count.DecrementCount();
    });
  }

  block_count.Wait();
  executor->Shutdown();

  //  Assert
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds.at(0));

  //  We should've run 1 time on the main thread, and 200 times on the
  //  workerThreads
  ASSERT_EQ(threadIds.size(), 251);
}

}  // namespace
}  // namespace windows
}  // namespace nearby
