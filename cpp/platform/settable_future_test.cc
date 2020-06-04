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

#include "platform/api/settable_future.h"

#include "platform/api/platform.h"
#include "platform/ptr.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

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

TEST(SettableFutureTest, SupportIntegralTypes) {
  auto p = platform::ImplementationPlatform::createSettableFuture<int>();
  p->set(5);
  ASSERT_EQ(p->get().exception(), Exception::kSuccess);
  ASSERT_EQ(p->get().result(), 5);
}

TEST(SettableFutureTest, SetExceptionIsPropagated) {
  auto p = platform::ImplementationPlatform::createSettableFuture<int>();
  p->setException({Exception::kIo});
  ASSERT_EQ(p->get().exception(), Exception::kIo);
}

TEST(SettableFutureTest, SupportEnum) {
  auto p = platform::ImplementationPlatform::createSettableFuture<TestEnum>();
  p->set(TestEnum::kValue1);
  ASSERT_EQ(p->get().exception(), Exception::kSuccess);
  ASSERT_EQ(p->get().result(), TestEnum::kValue1);
}

TEST(SettableFutureTest, SupportScopedEnum) {
  auto p =
      platform::ImplementationPlatform::createSettableFuture<ScopedTestEnum>();
  p->set(ScopedTestEnum::kValue1);
  ASSERT_EQ(p->get().exception(), Exception::kSuccess);
  ASSERT_EQ(p->get().result(), ScopedTestEnum::kValue1);
}

TEST(SettableFutureTest, SetTakesCopyOfValue) {
  // Default constructor is zero-initalizing all data in BigSizedStruct.
  BigSizedStruct v1;
  auto p = platform::ImplementationPlatform::createSettableFuture<
      BigSizedStruct>();
  v1.data[0] = 5;  // Changing value before calling set() will affect stored
  v1.data[7] = 3;  // value.
  p->set(v1);
  v1.data[1] = 6;  // Changing value after calling set() will not affect stored
  v1.data[5] = 4;  // value.
  ASSERT_EQ(p->get().exception(), Exception::kSuccess);
  BigSizedStruct v2 = p->get().result();
  ASSERT_NE(v1, v2);
  v1.data[1] = 0;
  v1.data[5] = 0;
  ASSERT_EQ(v2, v1);
}

}  // namespace nearby
}  // namespace location
