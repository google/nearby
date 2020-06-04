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

#include "platform_v2/public/future.h"

#include "platform_v2/public/single_thread_executor.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace location {
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
  executor.Execute([&future](){
    absl::SleepFor(absl::Milliseconds(500));
    future.Set(10);
  });
  auto response = future.Get();
  absl::Duration blocked_duration = absl::Now() - start;
  EXPECT_EQ(response.result(), 10);
  EXPECT_GE(blocked_duration, absl::Milliseconds(500));
}

}  // namespace nearby
}  // namespace location
