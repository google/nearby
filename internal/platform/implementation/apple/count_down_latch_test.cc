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

#include "internal/platform/implementation/apple/count_down_latch.h"

#include "gtest/gtest.h"
#include "thread/fiber/fiber.h"

namespace nearby {
namespace apple {
namespace {

TEST(CountDownLatchTest, LatchAwaitCanWait) {
  CountDownLatch latch(1);
  std::atomic_bool done = false;

  thread::Fiber f([&done, &latch] {
    done = true;
    latch.CountDown();
  });
  f.Join();

  latch.Await();
  EXPECT_TRUE(done);
}

TEST(CountDownLatchTest, LatchExtraCountDownIgnored) {
  CountDownLatch latch(1);
  std::atomic_bool done = false;

  thread::Fiber f([&done, &latch] {
    done = true;
    latch.CountDown();
    latch.CountDown();
    latch.CountDown();
  });
  f.Join();

  latch.Await();
  EXPECT_TRUE(done);
}

TEST(CountDownLatchTest, LatchAwaitWithTimeoutCanExpire) {
  CountDownLatch latch(1);

  auto response = latch.Await(absl::Milliseconds(100));

  EXPECT_TRUE(response.ok());
  EXPECT_FALSE(response.result());
}

TEST(CountDownLatchTest, InitialCountZero_AwaitDoesNotBlock) {
  CountDownLatch latch(0);

  auto response = latch.Await();

  EXPECT_TRUE(response.Ok());
}

TEST(CountDownLatchTest, InitialCountNegative_AwaitDoesNotBlock) {
  CountDownLatch latch(-1);

  auto response = latch.Await();

  EXPECT_TRUE(response.Ok());
}

}  // namespace
}  // namespace apple
}  // namespace nearby
