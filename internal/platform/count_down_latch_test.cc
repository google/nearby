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

#include "internal/platform/count_down_latch.h"

#include "gtest/gtest.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace {

TEST(CountDownLatch, ConstructorDestructorWorks) { CountDownLatch latch(1); }

TEST(CountDownLatch, LatchAwaitCanWait) {
  CountDownLatch latch(1);
  SingleThreadExecutor executor;
  std::atomic_bool done = false;
  executor.Execute([&done, &latch]() {
    done = true;
    latch.CountDown();
  });
  latch.Await();
  EXPECT_TRUE(done);
}

TEST(CountDownLatch, LatchExtraCountDownIgnored) {
  CountDownLatch latch(1);
  SingleThreadExecutor executor;
  std::atomic_bool done = false;
  executor.Execute([&done, &latch]() {
    done = true;
    latch.CountDown();
    latch.CountDown();
    latch.CountDown();
  });
  latch.Await();
  EXPECT_TRUE(done);
}

TEST(CountDownLatch, LatchAwaitWithTimeoutCanExpire) {
  CountDownLatch latch(1);
  SingleThreadExecutor executor;
  auto response = latch.Await(absl::Milliseconds(100));
  EXPECT_FALSE(response);
}

TEST(CountDownLatch, InitialCountZero_AwaitDoesNotBlock) {
  CountDownLatch latch(0);

  auto response = latch.Await();

  EXPECT_TRUE(response.Ok());
}

}  // namespace
}  // namespace nearby
