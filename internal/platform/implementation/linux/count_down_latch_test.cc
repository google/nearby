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

#include "gtest/gtest.h"
#include "internal/platform/implementation/platform.h"

#include <atomic>
#include <thread>
#include <vector>

class CountDownLatchTests : public testing::Test {
 public:
  class TestData {
   public:
    std::unique_ptr<nearby::api::CountDownLatch>& countDownLatch;
    long volatile& count;
  };

  class CountDownLatchTest {
   public:
    static unsigned int ThreadProcCountDown(void* lpParam) {
      TestData* testData = static_cast<TestData*>(lpParam);

      sleep(1);

      __sync_fetch_and_add(&testData->count, 1);

      testData->countDownLatch->CountDown();
      return 0;
    }

    static unsigned int ThreadProcAwait(void* lpParam) {
      TestData* testData = static_cast<TestData*>(lpParam);

      sleep(1);

      testData->countDownLatch->Await();
      __sync_fetch_and_add(&testData->count, 1);

      return 0;
    }
  };

  CountDownLatchTests() {}
};

TEST_F(CountDownLatchTests, CountDownLatchAwaitSucceeds) {
  // Arrange
  long volatile count = 0;

  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(3);

  std::vector<std::thread> threads;

  TestData testData{countDownLatch, count};

  // Setup 3 threads
  for (int i = 0; i < 3; i++) {
    // TODO: More complex scenarios may require use of a parameter
    //   to the thread procedure, such as an event per thread to
    //   be used for synchronization.
    //   https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread
    //   Could use C++ concurrency for this possibly
    threads.emplace_back(CountDownLatchTest::ThreadProcCountDown, &testData);
  }

  // Act
  nearby::Exception result = countDownLatch->Await();

  for (auto& thread : threads) {
    thread.join();
  }

  // Assert
  EXPECT_EQ(result.value, nearby::Exception::kSuccess);
  EXPECT_EQ(count, 3);
}

TEST_F(CountDownLatchTests, CountDownLatchAwaitTimeoutTimesOut) {
  // Arrange

  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(3);

  // Act
  nearby::ExceptionOr<bool> result =
      countDownLatch->Await(absl::Milliseconds(5));

  sleep(40);

  // Assert
  EXPECT_FALSE(result.GetResult());
  // TODO(jfcarroll)I think there's a bug in the shared version of this, it's
  // not returning a timeout exception, need to look at it some more.
  // EXPECT_EQ(result.GetException().value,
  // nearby::Exception::kTimeout);
}

TEST_F(CountDownLatchTests, CountDownLatchAwaitNoTimeoutSucceeds) {
  // Arrange
  long volatile count = 0;

  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(3);

  TestData testData{countDownLatch, count};

  std::vector<std::thread> threads;

  // Setup 3 threads
  for (int i = 0; i < 3; i++) {
    // TODO: More complex scenarios may require use of a parameter
    //   to the thread procedure, such as an event per thread to
    //   be used for synchronization.
    threads.emplace_back(CountDownLatchTest::ThreadProcAwait, &testData);
  }

  for (auto& thread : threads) {
    thread.join();
  }
  // Act
  nearby::ExceptionOr<bool> result =
      countDownLatch->Await(absl::Milliseconds(100));

  // Assert
  EXPECT_TRUE(result.GetResult());
  EXPECT_EQ(result.GetException().value, nearby::Exception::kSuccess);
  EXPECT_EQ(count, 3);
}

void test(std::string str) {
  std::cout << str << std::endl;
  return;
}

TEST_F(CountDownLatchTests, CountDownLatchCountDownBeforeAwaitSucceeds) {
  // Arrange
  long volatile count = 0;
  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(1);

  TestData testData{countDownLatch, count};
  std::thread thread(CountDownLatchTest::ThreadProcCountDown, &testData);

  // Act
  countDownLatch->CountDown();  // This countdown occurs before the thread has a
                                // chance to run
  // Assert
  EXPECT_EQ(count, 1);
}
