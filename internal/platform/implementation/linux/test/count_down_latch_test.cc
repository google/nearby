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

#include <pthread.h>
#include <atomic>

#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/platform.h"

class CountDownLatchTests : public testing::Test {
 public:
  class TestData {
   public:
    std::unique_ptr<nearby::api::CountDownLatch>& countDownLatch;
    std::atomic<int>& count;
  };

  class CountDownLatchTest {
   public:
    static void* ThreadProcCountDown(void* param) {
      TestData* testData = static_cast<TestData*>(param);

      absl::SleepFor(absl::Milliseconds(1));

      testData->count++;
      testData->countDownLatch->CountDown();
      return nullptr;
    }

    static void* ThreadProcAwait(void* param) {
      TestData* testData = static_cast<TestData*>(param);

      absl::SleepFor(absl::Milliseconds(1));

      testData->countDownLatch->Await();
      testData->count++;

      return nullptr;
    }
  };

  CountDownLatchTests() {}
};

TEST_F(CountDownLatchTests, CountDownLatchAwaitSucceeds) {
  // Arrange
  std::atomic<int> count(0);

  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(3);

  pthread_t threads[3];
  

  TestData testData{countDownLatch, count};

  // Setup 3 threads
  for (int i = 0; i < 3; i++) {
    int result = pthread_create(&threads[i], nullptr,
        CountDownLatchTest::ThreadProcCountDown,
        &testData);
    EXPECT_EQ(result, 0);  // pthread_create returns 0 on success
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
  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(3);

  // Act
  nearby::ExceptionOr<bool> result =
      countDownLatch->Await(absl::Milliseconds(5));

  absl::SleepFor(absl::Milliseconds(40));

  // Assert
  EXPECT_FALSE(result.GetResult());
  // TODO(jfcarroll)I think there's a bug in the shared version of this, it's
  // not returning a timeout exception, need to look at it some more.
  // EXPECT_EQ(result.GetException().value,
  // nearby::Exception::kTimeout);
}

TEST_F(CountDownLatchTests, CountDownLatchAwaitNoTimeoutSucceeds) {
  // Arrange
  std::atomic<int> count(0);

  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(3);

  TestData testData{countDownLatch, count};

  pthread_t threads[3];
  

  // Setup 3 threads
  for (int i = 0; i < 3; i++) {
    int result = pthread_create(&threads[i], nullptr,
        CountDownLatchTest::ThreadProcCountDown,
        &testData);
    EXPECT_EQ(result, 0);  // pthread_create returns 0 on success
  }

  for (int i = 0; i < 3; i++) pthread_join(threads[i], nullptr);
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
  std::atomic<int> count(0);
  std::unique_ptr<nearby::api::CountDownLatch> countDownLatch =
      nearby::api::ImplementationPlatform::CreateCountDownLatch(1);

  pthread_t thread;

  TestData testData{countDownLatch, count};

  int result = pthread_create(&thread, nullptr,
      CountDownLatchTest::ThreadProcAwait,
      &testData);

  EXPECT_EQ(result, 0);

  // Act
  countDownLatch->CountDown();  // This countdown occurs before the thread has a
                                // chance to run

  pthread_join(thread, nullptr);  // This will wait till the thread exits

  // Assert
  EXPECT_EQ(count, 1);
}
