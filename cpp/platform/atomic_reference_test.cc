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

#include "platform/api/atomic_reference.h"

#include "platform/api/platform.h"
#include "platform/ptr.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace {

struct BigSizedStruct {
  int data[100]{};
};

enum TestEnum {
  kValue1 = 1,
  kValue2 = 2,
};

enum class ScopedTestEnum {
  kValue1 = 1,
  kValue2 = 2,
};

bool operator==(const BigSizedStruct& a, const BigSizedStruct& b) {
  return memcmp(a.data, b.data, sizeof(BigSizedStruct::data)) == 0;
}

bool operator!=(const BigSizedStruct& a, const BigSizedStruct& b) {
  return !(a == b);
}

}  // namespace

TEST(AtomicReferenceTest, SupportIntegralTypes) {
  auto p = platform::ImplementationPlatform::createAtomicReference<int>();
  p->set(5);
  ASSERT_EQ(p->get(), 5);
}

TEST(AtomicReferenceTest, SupportEnum) {
  auto p = platform::ImplementationPlatform::createAtomicReference<TestEnum>();
  p->set(TestEnum::kValue1);
  ASSERT_EQ(p->get(), TestEnum::kValue1);
}

TEST(AtomicReferenceTest, SupportScopedEnum) {
  auto p =
      platform::ImplementationPlatform::createAtomicReference<ScopedTestEnum>();
  p->set(ScopedTestEnum::kValue1);
  ASSERT_EQ(p->get(), ScopedTestEnum::kValue1);
}

TEST(AtomicReferenceTest, SetTakesCopyOfValue) {
  // Default constructor is zero-initalizing all data in BigSizedStruct.
  BigSizedStruct v1;
  auto p = platform::ImplementationPlatform::createAtomicReference<
      BigSizedStruct>();
  v1.data[0] = 5;  // Changing value before calling set() will affect stored
  v1.data[7] = 3;  // value.
  p->set(v1);
  v1.data[1] = 6;  // Changing value after calling set() will not affect stored
  v1.data[5] = 4;  // value.
  BigSizedStruct v2 = p->get();
  ASSERT_NE(v1, v2);
  v1.data[1] = 0;
  v1.data[5] = 0;
  ASSERT_EQ(v2, v1);
}

TEST(AtomicReferenceTest, SupportObjects) {
  std::string s{"test"};
  auto ref =
      platform::ImplementationPlatform::createAtomicReference<std::string>(s);
  ASSERT_EQ(s, ref->get());
}

}  // namespace nearby
}  // namespace location
