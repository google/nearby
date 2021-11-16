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

#include "platform/impl/windows/executor.h"

#include <algorithm>
#include <utility>

#include "platform/impl/windows/test_data.h"

#include "gtest/gtest.h"

TEST(ExecutorTests, SingleThreadedExecutorSucceeds) {
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  std::unique_ptr<location::nearby::windows::Executor> executor =
      std::make_unique<location::nearby::windows::Executor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  executor->Execute([&output, &threadIds]() {
    threadIds->push_back(GetCurrentThreadId());
    output.append(RUNNABLE_0_TEXT.c_str());
  });

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

  std::unique_ptr<location::nearby::windows::Executor> executor =
      std::make_unique<location::nearby::windows::Executor>();
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
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  std::unique_ptr<location::nearby::windows::Executor> executor =
      std::make_unique<location::nearby::windows::Executor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  executor->Execute(nullptr);
  executor->Execute([&output, &threadIds]() {
    threadIds->push_back(GetCurrentThreadId());
    output.append(RUNNABLE_0_TEXT.c_str());
  });
  executor->Execute(nullptr);

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
  // Arrange
  std::string expected(RUNNABLE_ALL_TEXT.c_str());

  std::unique_ptr<location::nearby::windows::Executor> executor =
      std::make_unique<location::nearby::windows::Executor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  for (int index = 0; index < 5; index++) {
    executor->Execute([&output, &threadIds, index]() {
      threadIds->push_back(GetCurrentThreadId());
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "%s%d, ", RUNNABLE_TEXT.c_str(), index);
      output.append(std::string(buffer));
    });
  }

  executor->Shutdown();

  //  Assert
  //  We should've run 1 time on the main thread, and 5 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 6);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  auto workerThreadId = threadIds->at(1);
  for (int index = 1; index < threadIds->size(); index++) {
    ASSERT_EQ(threadIds->at(index), workerThreadId);
  }

  //  We should of run them in the order submitted
  ASSERT_EQ(output, expected);
}

TEST(ExecutorTests, MultiThreadedExecutorSingleTaskSucceeds) {
  //  Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  std::unique_ptr<location::nearby::windows::Executor> executor =
      std::make_unique<location::nearby::windows::Executor>(2);

  //  Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  std::shared_ptr<std::string> output = std::make_shared<std::string>();

  threadIds->push_back(GetCurrentThreadId());

  //  Act
  executor->Execute([output, &threadIds]() {
    threadIds->push_back(GetCurrentThreadId());
    output->append(RUNNABLE_0_TEXT.c_str());
  });

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
  //  Arrange
  std::unique_ptr<location::nearby::windows::Executor> executor =
      std::make_unique<location::nearby::windows::Executor>(2);

  //  Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  std::shared_ptr<std::string> output = std::make_shared<std::string>();

  threadIds->push_back(GetCurrentThreadId());

  //  Act
  for (int index = 0; index < 5; index++) {
    executor->Execute([&output, &threadIds, index]() {
      threadIds->push_back(GetCurrentThreadId());
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "%s %d, ", RUNNABLE_TEXT.c_str(), index);
      output->append(std::string(buffer));
    });
  }

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

  std::unique_ptr<location::nearby::windows::Executor> executor =
      std::make_unique<location::nearby::windows::Executor>(2);

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

TEST(ExecutorTests, MultiThreadedExecutorNegativeThreadsThrows) {
  //  Arrange
  //  Act
  //  Assert
  EXPECT_THROW(
      {
        try {
          auto result =
              std::make_unique<location::nearby::windows::Executor>(-1);
        } catch (const std::invalid_argument::exception& e) {
          // and this tests that it has the correct message
          EXPECT_STREQ(INVALID_ARGUMENT_TEXT, e.what());
          throw;
        }
      },
      std::invalid_argument);
}

TEST(ExecutorTests, MultiThreadedExecutorTooManyThreadsThrows) {
  //  Arrange
  //  Act
  //  Assert
  EXPECT_THROW(
      {
        try {
          auto result =
              std::make_unique<location::nearby::windows::Executor>(65);
        } catch (const location::nearby::windows::ThreadPoolException& e) {
          // and this tests that it has the correct message
          EXPECT_STREQ(THREADPOOL_MAX_SIZE_TEXT, e.what());
          throw;
        }
      },
      location::nearby::windows::ThreadPoolException);
}

TEST(ExecutorTests,
     MultiThreadedExecutorMultipleTasksLargeNumberOfThreadsSucceeds) {
  //  Arrange
  std::unique_ptr<location::nearby::windows::Executor> executor =
      std::make_unique<location::nearby::windows::Executor>(
          MAXIMUM_WAIT_OBJECTS - 1);

  //  Container to note threads that ran
  std::vector<DWORD> threadIds = std::vector<DWORD>();

  std::shared_ptr<std::string> output = std::make_shared<std::string>();

  threadIds.push_back(GetCurrentThreadId());

  CRITICAL_SECTION testCriticalSection;
  InitializeCriticalSection(&testCriticalSection);

  //  Act
  for (int index = 0; index < 250; index++) {
    executor->Execute(
        [output, &threadIds, index, &testCriticalSection]() mutable {
          DWORD id = GetCurrentThreadId();

          EnterCriticalSection(&testCriticalSection);

          threadIds.push_back(id);
          output->append(RUNNABLE_TEXT);
          output->append(std::to_string(index));
          output->append(RUNNABLE_SEPARATOR_TEXT);

          LeaveCriticalSection(&testCriticalSection);
          // Using rand since this is in a critical section
          // and windows doesn't have a rand_r anyway
          auto sleepTime = (std::rand() % 101) + 1;  //  NOLINT

          Sleep(sleepTime);
        });
  }

  executor->Shutdown();
  DeleteCriticalSection(&testCriticalSection);

  //  Assert
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds.at(0));

  std::sort(threadIds.begin(), threadIds.end());
  int64_t uniqueIds =
      std::unique(threadIds.begin(), threadIds.end()) - threadIds.begin();

  ASSERT_EQ(uniqueIds, 64);
  //  We should've run 1 time on the main thread, and 200 times on the
  //  workerThreads
  ASSERT_EQ(threadIds.size(), 251);
}
