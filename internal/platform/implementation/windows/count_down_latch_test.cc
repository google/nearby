// Copyright 2020 Google LLC
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

#include "internal/platform/implementation/shared/count_down_latch.h"

#include <Windows.h>

#include "gtest/gtest.h"
#include "internal/platform/implementation/platform.h"

class CountDownLatchTests : public testing::Test {
 public:
  class TestData {
   public:
    std::unique_ptr<nearby::api::CountDownLatch>& countDownLatch;
    LONG volatile& count;
  };

  class CountDownLatchTest {
   public:
    static DWORD ThreadProcCountDown(LPVOID lpParam) {
      TestData* testData = static_cast<TestData*>(lpParam);

      Sleep(1);

      InterlockedIncrement(&testData->count);
      testData->countDownLatch->CountDown();
      return 0;
    }

    static DWORD ThreadProcAwait(LPVOID lpParam) {
      TestData* testData = static_cast<TestData*>(lpParam);

      Sleep(1);

      testData->countDownLatch->Await();
      InterlockedIncrement(&testData->count);

      return 0;
    }
  };

  CountDownLatchTests() {}
};

TEST_F(CountDownLatchTests, CountDownLatchAwaitSucceeds) {
  // Arrange
  LONG volatile count = 0;

  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(3);

  HANDLE hThreads[3];
  DWORD dwThreadID;

  TestData testData{countDownLatch, count};

  // Setup 3 threads
  for (int i = 0; i < 3; i++) {
    // TODO: More complex scenarios may require use of a parameter
    //   to the thread procedure, such as an event per thread to
    //   be used for synchronization.
    hThreads[i] = CreateThread(
        NULL,                                     // default security
        0,                                        // default stack size
        CountDownLatchTest::ThreadProcCountDown,  // name of the thread function
        &testData,                                // no thread parameters
        0,                                        // default startup flags
        &dwThreadID);

    EXPECT_TRUE(hThreads[i] != NULL);
  }

  // Act
  nearby::Exception result = countDownLatch->Await();

  //
  // Assert
  EXPECT_EQ(result.value, nearby::Exception::kSuccess);
  EXPECT_EQ(count, 3);
}

TEST_F(CountDownLatchTests, CountDownLatchAwaitTimeoutTimesOut) {
  // Arrange
  LONG volatile count = 0;

  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(3);

  // Act
  nearby::ExceptionOr<bool> result =
      countDownLatch->Await(absl::Milliseconds(5));

  Sleep(40);

  // Assert
  EXPECT_FALSE(result.GetResult());
  // TODO(jfcarroll)I think there's a bug in the shared version of this, it's
  // not returning a timeout exception, need to look at it some more.
  // EXPECT_EQ(result.GetException().value,
  // nearby::Exception::kTimeout);
}

TEST_F(CountDownLatchTests, CountDownLatchAwaitNoTimeoutSucceeds) {
  // Arrange
  LONG volatile count = 0;

  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(3);

  TestData testData{countDownLatch, count};

  HANDLE hThreads[3];
  DWORD dwThreadID;

  // Setup 3 threads
  for (int i = 0; i < 3; i++) {
    // TODO: More complex scenarios may require use of a parameter
    //   to the thread procedure, such as an event per thread to
    //   be used for synchronization.
    hThreads[i] = CreateThread(
        NULL,                                     // default security
        0,                                        // default stack size
        CountDownLatchTest::ThreadProcCountDown,  // name of the thread function
        &testData,                                // no thread parameters
        0,                                        // default startup flags
        &dwThreadID);

    EXPECT_TRUE(hThreads[i] != NULL);
  }

  WaitForMultipleObjects(3, hThreads, true, INFINITE);
  // Act
  nearby::ExceptionOr<bool> result =
      countDownLatch->Await(absl::Milliseconds(100));

  // Assert
  EXPECT_TRUE(result.GetResult());
  EXPECT_EQ(result.GetException().value, nearby::Exception::kSuccess);
  EXPECT_EQ(count, 3);
}

TEST_F(CountDownLatchTests, CountDownLatchCountDownBeforeAwaitSucceeds) {
  // Arrange
  LONG volatile count = 0;
  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(1);

  HANDLE hThread;
  DWORD dwThreadID;

  TestData testData{countDownLatch, count};

  hThread = CreateThread(
      NULL,                                 // default security
      0,                                    // default stack size
      CountDownLatchTest::ThreadProcAwait,  // name of the thread function
      &testData,                            // no thread parameters
      0,                                    // default startup flags
      &dwThreadID);

  EXPECT_TRUE(hThread != NULL);

  // Act
  countDownLatch->CountDown();  // This countdown occurs before the thread has a
                                // chance to run

  if (hThread != NULL) {
    WaitForSingleObject(hThread,
                        INFINITE);  // This will wait till the thread exits
  }

  // Assert
  EXPECT_EQ(count, 1);
}
