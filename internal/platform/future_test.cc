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

#include "internal/platform/future.h"

#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/direct_executor.h"
#include "internal/platform/exception.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {

namespace {

enum TestEnum {
  kValue1 = 1,
  kValue2 = 2,
};

enum class ScopedTestEnum {
  kValue1 = 1,
  kValue2 = 2,
};

struct BigSizedStruct {
  int data[100]{};
};

bool operator==(const BigSizedStruct& a, const BigSizedStruct& b) {
  return memcmp(a.data, b.data, sizeof(BigSizedStruct::data)) == 0;
}

bool operator!=(const BigSizedStruct& a, const BigSizedStruct& b) {
  return !(a == b);
}

}  // namespace

TEST(FutureTest, SupportIntegralTypes) {
  Future<int> future;
  future.Set(5);
  EXPECT_EQ(future.Get().exception(), Exception::kSuccess);
  EXPECT_EQ(future.Get().result(), 5);
}

TEST(FutureTest, SetExceptionIsPropagated) {
  Future<int> future;
  future.SetException({Exception::kIo});
  EXPECT_EQ(future.Get().exception(), Exception::kIo);
}

TEST(FutureTest, SupportEnum) {
  Future<TestEnum> future;
  future.Set(TestEnum::kValue1);
  EXPECT_EQ(future.Get().exception(), Exception::kSuccess);
  EXPECT_EQ(future.Get().result(), TestEnum::kValue1);
}

TEST(FutureTest, SupportScopedEnum) {
  Future<ScopedTestEnum> future;
  future.Set(ScopedTestEnum::kValue1);
  EXPECT_EQ(future.Get().exception(), Exception::kSuccess);
  EXPECT_EQ(future.Get().result(), ScopedTestEnum::kValue1);
}

TEST(FutureTest, SetTakesCopyOfValue) {
  // Default constructor is zero-initalizing all data in BigSizedStruct.
  BigSizedStruct v1;
  Future<BigSizedStruct> future;
  v1.data[0] = 5;  // Changing value before calling Set() will affect stored
  v1.data[7] = 3;  // value.
  future.Set(v1);
  v1.data[1] = 6;  // Changing value after calling Set() will not affect stored
  v1.data[5] = 4;  // value.
  EXPECT_EQ(future.Get().exception(), Exception::kSuccess);
  BigSizedStruct v2 = future.Get().result();
  EXPECT_NE(v1, v2);
  v1.data[1] = 0;
  v1.data[5] = 0;
  EXPECT_EQ(v2, v1);
}

TEST(FutureTest, SetsExceptionOnTimeout) {
  Future<int> future;
  EXPECT_EQ(future.Get(absl::Milliseconds(100)).exception(),
            Exception::kTimeout);
}

TEST(FutureTest, GetBlocksWhenNotReady) {
  Future<int> future;
  SingleThreadExecutor executor;
  absl::Time start = absl::Now();
  executor.Execute([&future]() {
    absl::SleepFor(absl::Milliseconds(500));
    future.Set(10);
  });
  auto response = future.Get();
  absl::Duration blocked_duration = absl::Now() - start;
  EXPECT_EQ(response.result(), 10);
  EXPECT_GE(blocked_duration, absl::Milliseconds(500));
}

TEST(FutureTest, CallsListenerOnSet) {
  constexpr int kValue = 1000;
  Future<int> future;
  int call_count = 0;
  {
    SingleThreadExecutor executor;
    future.AddListener(
        [&](ExceptionOr<int> result) {
          ASSERT_TRUE(result.ok());
          ASSERT_EQ(result.GetResult(), kValue);
          ++call_count;
        },
        &executor);

    future.Set(kValue);
    // `executor` leaves scope, the destructor waits for tasks to complete
  }

  EXPECT_EQ(call_count, 1);
}

TEST(FutureTest, CallsAllListenersOnSet) {
  constexpr int kValue = 1000;
  Future<int> future;
  int call_count_listener_1 = 0;
  int call_count_listener_2 = 0;
  {
    SingleThreadExecutor executor;
    future.AddListener(
        [&](ExceptionOr<int> result) {
          ASSERT_TRUE(result.ok());
          ASSERT_EQ(result.GetResult(), kValue);
          ++call_count_listener_1;
        },
        &executor);
    future.AddListener(
        [&](ExceptionOr<int> result) {
          ASSERT_TRUE(result.ok());
          ASSERT_EQ(result.GetResult(), kValue);
          ++call_count_listener_2;
        },
        &executor);

    future.Set(kValue);
    // `executor` leaves scope, the destructor waits for tasks to complete
  }

  EXPECT_EQ(call_count_listener_1, 1);
  EXPECT_EQ(call_count_listener_2, 1);
}

TEST(FutureTest, AddListenerWhenAlreadySetCallsCallback) {
  constexpr int kValue = 1000;
  Future<int> future;
  int call_count = 0;
  future.Set(kValue);
  {
    SingleThreadExecutor executor;
    future.AddListener(
        [&](ExceptionOr<int> result) {
          ASSERT_TRUE(result.ok());
          ASSERT_EQ(result.GetResult(), kValue);
          ++call_count;
        },
        &executor);
    // `executor` leaves scope, the destructor waits for tasks to complete
  }

  EXPECT_EQ(call_count, 1);
}

TEST(FutureTest, CallsListenerOnSetException) {
  constexpr Exception kException = {Exception::kFailed};
  Future<int> future;
  int call_count = 0;
  {
    SingleThreadExecutor executor;
    future.AddListener(
        [&](ExceptionOr<int> result) {
          ASSERT_FALSE(result.ok());
          ASSERT_EQ(result.GetException(), kException);
          ++call_count;
        },
        &executor);

    future.SetException(kException);
    // `executor` leaves scope, the destructor waits for tasks to complete
  }

  EXPECT_EQ(call_count, 1);
}

TEST(FutureTest, AddListenerWhenAlreadySetExceptionCallsCallback) {
  constexpr Exception kException = {Exception::kFailed};
  Future<int> future;
  int call_count = 0;
  future.SetException(kException);
  {
    SingleThreadExecutor executor;

    future.AddListener(
        [&](ExceptionOr<int> result) {
          ASSERT_FALSE(result.ok());
          ASSERT_EQ(result.GetException(), kException);
          ++call_count;
        },
        &executor);

    // `executor` leaves scope, the destructor waits for tasks to complete
  }

  EXPECT_EQ(call_count, 1);
}

TEST(FutureTest, TimeoutSetsException) {
  Future<int> future(absl::Milliseconds(10));

  EXPECT_EQ(future.Get().exception(), Exception::kTimeout);
}

TEST(FutureTest, TimeoutCallsListeners) {
  Future<int> future(absl::Milliseconds(10));
  CountDownLatch latch(1);
  future.AddListener(
      [&](ExceptionOr<int> result) {
        ASSERT_FALSE(result.ok());
        ASSERT_EQ(result.exception(), Exception::kTimeout);
        latch.CountDown();
      },
      &DirectExecutor::GetInstance());

  EXPECT_TRUE(latch.Await().Ok());

  EXPECT_EQ(future.Get().exception(), Exception::kTimeout);
}

TEST(FutureTest, SetValueBeforeTimeout) {
  Future<int> future(absl::Minutes(1));

  future.Set(5);

  EXPECT_EQ(future.Get().result(), 5);
}

TEST(FutureTest, SetExceptionBeforeTimeout) {
  Future<int> future(absl::Minutes(1));

  future.SetException({Exception::kExecution});

  EXPECT_EQ(future.Get().exception(), Exception::kExecution);
}

}  // namespace nearby
