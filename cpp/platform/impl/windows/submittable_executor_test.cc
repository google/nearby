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
#include "platform/impl/windows/submittable_executor.h"

#include <utility>

#include "platform/impl/windows/test_data.h"

#include "gtest/gtest.h"

TEST(SubmittableExecutorTests, SingleThreadedExecuteSucceeds) {
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  std::unique_ptr<location::nearby::windows::SubmittableExecutor>
      submittableExecutor =
          std::make_unique<location::nearby::windows::SubmittableExecutor>();
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

TEST(SubmittableExecutorTests, SingleThreadedExecuteAfterShutdownFails) {
  // Arrange
  std::string expected("");

  std::unique_ptr<location::nearby::windows::SubmittableExecutor>
      submittableExecutor =
          std::make_unique<location::nearby::windows::SubmittableExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  submittableExecutor->Shutdown();

  // Act
  submittableExecutor->Execute([&output, &threadIds]() {
    threadIds->push_back(GetCurrentThreadId());
    output.append(RUNNABLE_0_TEXT.c_str());
  });

  //  Assert
  //  We should've run 1 time on the main thread, and 0 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 1);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(SubmittableExecutorTests, SingleThreadedDoSubmitSucceeds) {
  // Arrange
  std::string expected(RUNNABLE_0_TEXT.c_str());

  std::unique_ptr<location::nearby::windows::SubmittableExecutor>
      submittableExecutor =
          std::make_unique<location::nearby::windows::SubmittableExecutor>();
  std::string output = std::string();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  auto result = submittableExecutor->DoSubmit([&output, &threadIds]() {
    threadIds->push_back(GetCurrentThreadId());
    output.append(RUNNABLE_0_TEXT.c_str());
  });

  submittableExecutor->Shutdown();

  //  Assert
  //  We should've said we were going to run this one
  ASSERT_TRUE(result);
  //  We should've run 1 time on the main thread, and 1 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 2);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(output, expected);
}

TEST(SubmittableExecutorTests,
     SingleThreadedDoSubmitAfterShutdownReturnsFalse) {
  // Arrange
  std::string expected("");

  std::unique_ptr<location::nearby::windows::SubmittableExecutor>
      submittableExecutor =
          std::make_unique<location::nearby::windows::SubmittableExecutor>();
  std::unique_ptr<std::string> output = std::make_unique<std::string>();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  submittableExecutor->Shutdown();

  // Act
  auto result = submittableExecutor->DoSubmit([&output, &threadIds]() {
    threadIds->push_back(GetCurrentThreadId());
    output->append(RUNNABLE_0_TEXT.c_str());
  });

  //  Assert
  //  We should've said we were going to run this one
  ASSERT_FALSE(result);
  //  We should've run 1 time on the main thread, and 1 times on the
  //  workerThread
  ASSERT_EQ(threadIds->size(), 1);
  //  We should still be on the main thread
  ASSERT_EQ(GetCurrentThreadId(), threadIds->at(0));
  //  We should've run all runnables on the worker thread
  ASSERT_EQ(*output.get(), expected);
}

TEST(SubmittableExecutorTests, SingleThreadedExecuteMultipleTasksSucceeds) {
  // Arrange
  std::string expected(RUNNABLE_ALL_TEXT.c_str());

  std::unique_ptr<location::nearby::windows::SubmittableExecutor>
      submittableExecutor =
          std::make_unique<location::nearby::windows::SubmittableExecutor>();
  std::unique_ptr<std::string> output = std::make_unique<std::string>();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  for (int index = 0; index < 5; index++) {
    submittableExecutor->Execute([&output, &threadIds, index]() {
      threadIds->push_back(GetCurrentThreadId());
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "%s%d, ", RUNNABLE_TEXT.c_str(), index);
      output->append(std::string(buffer));
    });
  }

  submittableExecutor->Shutdown();

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
  ASSERT_EQ(*output.get(), expected);
}

TEST(SubmittableExecutorTests, SingleThreadedDoSubmitMultipleTasksSucceeds) {
  // Arrange
  std::string expected(RUNNABLE_ALL_TEXT.c_str());

  std::unique_ptr<location::nearby::windows::SubmittableExecutor>
      submittableExecutor =
          std::make_unique<location::nearby::windows::SubmittableExecutor>();
  std::unique_ptr<std::string> output = std::make_unique<std::string>();
  // Container to note threads that ran
  std::unique_ptr<std::vector<DWORD>> threadIds =
      std::make_unique<std::vector<DWORD>>();

  threadIds->push_back(GetCurrentThreadId());

  // Act
  bool result = true;
  for (int index = 0; index < 5; index++) {
    result &= submittableExecutor->DoSubmit([&output, &threadIds, index]() {
      threadIds->push_back(GetCurrentThreadId());
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "%s%d, ", RUNNABLE_TEXT.c_str(), index);
      output->append(std::string(buffer));
    });
  }

  submittableExecutor->Shutdown();

  //  Assert
  //  All of these should have submitted
  ASSERT_TRUE(result);
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
  ASSERT_EQ(*output.get(), expected);
}
