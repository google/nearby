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

#include "internal/platform/implementation/windows/scheduled_executor.h"

#include <atomic>
#include <chrono>  // NOLINT
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/windows/test_data.h"

namespace nearby {
namespace windows {
namespace {

constexpr absl::Duration kWaitTimeout = absl::Milliseconds(2000);

TEST(ScheduledExecutorTest, ExecuteSucceeds) {
  absl::Notification notification;
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  auto submittableExecutor = std::make_unique<ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  submittableExecutor->Execute([&]() {
    threadIds->push_back(GetCurrentThreadId());
    output.append(RUNNABLE_0_TEXT.c_str());
    notification.Notify();
  });

  ASSERT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
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

TEST(ScheduledExecutorTest, ScheduleSucceeds) {
  absl::Notification notification;
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  auto submittableExecutor = std::make_unique<ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  std::chrono::system_clock::time_point timeExecuted;

  // Act
  submittableExecutor->Schedule(
      [&]() {
        timeExecuted = std::chrono::system_clock::now();
        threadIds->push_back(GetCurrentThreadId());
        output.append(RUNNABLE_0_TEXT.c_str());
        notification.Notify();
      },
      absl::Milliseconds(50));

  ASSERT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  submittableExecutor->Shutdown();

  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(ScheduledExecutorTest, CancelSucceeds) {
  absl::Notification notification;
  // Arrange
  std::string expected("");

  auto submittableExecutor = std::make_unique<ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  auto cancelable = submittableExecutor->Schedule(
      [&]() {
        threadIds->push_back(GetCurrentThreadId());
        output.append(RUNNABLE_0_TEXT.c_str());
        notification.Notify();
      },
      absl::Milliseconds(1000));

  auto actual = cancelable->Cancel();

  EXPECT_FALSE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  submittableExecutor->Shutdown();

  // Assert
  ASSERT_TRUE(actual);
  ASSERT_EQ(threadIds->size(), 1);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(ScheduledExecutorTest, CancelAfterStartedFails) {
  absl::Notification notification;
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  auto submittableExecutor = std::make_unique<ScheduledExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  auto cancelable = submittableExecutor->Schedule(
      [&]() {
        threadIds->push_back(GetCurrentThreadId());
        output.append(RUNNABLE_0_TEXT.c_str());
        notification.Notify();
      },
      absl::Milliseconds(100));

  absl::SleepFor(absl::Milliseconds(500));
  auto actual = cancelable->Cancel();

  ASSERT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  submittableExecutor->Shutdown();

  // Assert
  ASSERT_FALSE(actual);
  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(ScheduledExecutorTest, SingleThreadedExecuteAfterShutdownFails) {
  auto executor = std::make_unique<ScheduledExecutor>();
  std::string output;
  std::vector<DWORD> thread_ids;

  thread_ids.push_back(GetCurrentThreadId());
  executor->Shutdown();

  EXPECT_FALSE(executor->Execute([&output, &thread_ids]() {
    thread_ids.push_back(GetCurrentThreadId());
    output.append(RUNNABLE_0_TEXT.c_str());
  }));

  ASSERT_EQ(thread_ids.size(), 1);
  ASSERT_EQ(GetCurrentThreadId(), thread_ids.at(0));
  ASSERT_EQ(output, "");
}

TEST(ScheduledExecutorTest, SingleThreadedExecuteNullSucceeds) {
  absl::Notification notification;
  std::string expected(RUNNABLE_0_TEXT.c_str());

  auto executor = std::make_unique<ScheduledExecutor>();
  std::string output;
  std::vector<DWORD> thread_ids;

  thread_ids.push_back(GetCurrentThreadId());

  EXPECT_FALSE(executor->Execute(nullptr));
  EXPECT_TRUE(executor->Execute([&]() {
    thread_ids.push_back(GetCurrentThreadId());
    output.append(RUNNABLE_0_TEXT.c_str());
    notification.Notify();
  }));
  EXPECT_FALSE(executor->Execute(nullptr));

  ASSERT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  executor->Shutdown();

  ASSERT_EQ(thread_ids.size(), 2);
  ASSERT_EQ(GetCurrentThreadId(), thread_ids.at(0));
  ASSERT_EQ(output, expected);
}

TEST(ScheduledExecutorTest, SingleThreadedExecuteMultipleTasksSucceeds) {
  absl::BlockingCounter block_count(5);
  std::string expected(RUNNABLE_ALL_TEXT.c_str());

  auto executor = std::make_unique<ScheduledExecutor>();
  std::string output;
  std::vector<DWORD> thread_ids;
  DWORD parent_thread = GetCurrentThreadId();

  for (int index = 0; index < 5; index++) {
    executor->Execute([&, index]() {
      thread_ids.push_back(GetCurrentThreadId());
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "%s%d, ", RUNNABLE_TEXT.c_str(), index);
      output.append(std::string(buffer));
      block_count.DecrementCount();
    });
  }

  block_count.Wait();
  executor->Shutdown();

  ASSERT_EQ(thread_ids.size(), 5);
  ASSERT_EQ(GetCurrentThreadId(), parent_thread);
  DWORD worker_thread_id = thread_ids.at(0);
  for (size_t index = 0; index < thread_ids.size(); index++) {
    ASSERT_EQ(thread_ids.at(index), worker_thread_id);
  }
  ASSERT_EQ(output, expected);
}

TEST(ScheduledExecutorTest, MultiThreadedExecuteSingleTaskSucceeds) {
  absl::Notification notification;
  std::string expected(RUNNABLE_0_TEXT.c_str());

  auto executor = std::make_unique<ScheduledExecutor>(2);
  std::vector<DWORD> thread_ids;
  auto output = std::make_shared<std::string>();

  thread_ids.push_back(GetCurrentThreadId());

  executor->Execute([&, output]() {
    thread_ids.push_back(GetCurrentThreadId());
    output->append(RUNNABLE_0_TEXT.c_str());
    notification.Notify();
  });

  ASSERT_TRUE(notification.WaitForNotificationWithTimeout(kWaitTimeout));
  executor->Shutdown();

  ASSERT_EQ(thread_ids.size(), 2);
  ASSERT_EQ(GetCurrentThreadId(), thread_ids.at(0));
  ASSERT_EQ(*output, expected);
}

TEST(ScheduledExecutorTest, MultiThreadedExecuteMultipleTasksSucceeds) {
  absl::BlockingCounter block_count(5);

  auto executor = std::make_unique<ScheduledExecutor>(2);
  absl::Mutex mutex;
  std::vector<DWORD> thread_ids;
  thread_ids.push_back(GetCurrentThreadId());

  for (int index = 0; index < 5; index++) {
    executor->Execute([&]() {
      {
        absl::MutexLock lock(mutex);
        thread_ids.push_back(GetCurrentThreadId());
      }
      block_count.DecrementCount();
    });
  }

  block_count.Wait();
  executor->Shutdown();

  ASSERT_EQ(thread_ids.size(), 6);
  ASSERT_EQ(GetCurrentThreadId(), thread_ids.at(0));
}

TEST(ScheduledExecutorTest, MultiThreadedExecuteSingleTaskAfterShutdownFails) {
  auto executor = std::make_unique<ScheduledExecutor>(2);
  std::vector<DWORD> thread_ids;
  auto output = std::make_shared<std::string>();

  thread_ids.push_back(GetCurrentThreadId());
  executor->Shutdown();

  EXPECT_FALSE(executor->Execute([output, &thread_ids]() {
    thread_ids.push_back(GetCurrentThreadId());
    output->append(RUNNABLE_0_TEXT.c_str());
  }));

  ASSERT_EQ(thread_ids.size(), 1);
  ASSERT_EQ(GetCurrentThreadId(), thread_ids.at(0));
  ASSERT_EQ(*output, "");
}

TEST(ScheduledExecutorTest,
     MultiThreadedExecuteMultipleTasksLargeNumberOfThreadsSucceeds) {
  absl::BlockingCounter block_count(250);

  auto executor = std::make_unique<ScheduledExecutor>(32);
  std::vector<DWORD> thread_ids;
  thread_ids.push_back(GetCurrentThreadId());
  absl::Mutex mutex;

  for (int index = 0; index < 250; index++) {
    executor->Execute([&]() mutable {
      DWORD id = GetCurrentThreadId();
      DWORD sleep_time = 1;
      {
        absl::MutexLock lock(mutex);
        thread_ids.push_back(id);
        sleep_time = (std::rand() % 101) + 1;  // NOLINT
      }
      Sleep(sleep_time);
      block_count.DecrementCount();
    });
  }

  block_count.Wait();
  executor->Shutdown();

  ASSERT_EQ(GetCurrentThreadId(), thread_ids.at(0));
  ASSERT_EQ(thread_ids.size(), 251);
}

TEST(ScheduledExecutorTest, ExecuteAndScheduleDuringShutdownDoNotDeadlock) {
  absl::Notification started;
  auto executor = std::make_unique<ScheduledExecutor>();
  std::atomic_bool nested_ran = false;
  std::shared_ptr<api::Cancelable> nested_scheduled;

  executor->Execute([&]() {
    started.Notify();
    Sleep(50);
    executor->Execute([&]() { nested_ran.store(true); });
    nested_scheduled = executor->Schedule([&]() { nested_ran.store(true); },
                                          absl::Milliseconds(10));
  });

  ASSERT_TRUE(started.WaitForNotificationWithTimeout(absl::Seconds(5)));
  executor->Shutdown();
  executor->Shutdown();
  EXPECT_FALSE(nested_ran.load());
  EXPECT_EQ(nested_scheduled, nullptr);
}

}  // namespace
}  // namespace windows
}  // namespace nearby
