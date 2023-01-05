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

#include "internal/platform/implementation/apple/atomic_boolean.h"

#include "gtest/gtest.h"
#include "thread/fiber/fiber.h"

namespace nearby {
namespace apple {
namespace {

TEST(AtomicBooleanTest, SetOnSameThread) {
  AtomicBoolean atomic_boolean_{false};

  EXPECT_EQ(false, atomic_boolean_.Get());

  atomic_boolean_.Set(true);
  EXPECT_EQ(true, atomic_boolean_.Get());
}

TEST(AtomicBooleanTest, MultipleSetGetOnSameThread) {
  AtomicBoolean atomic_boolean_{false};

  EXPECT_EQ(false, atomic_boolean_.Get());

  atomic_boolean_.Set(true);
  EXPECT_EQ(true, atomic_boolean_.Get());

  atomic_boolean_.Set(true);
  EXPECT_EQ(true, atomic_boolean_.Get());

  atomic_boolean_.Set(false);
  EXPECT_EQ(false, atomic_boolean_.Get());

  atomic_boolean_.Set(true);
  EXPECT_EQ(true, atomic_boolean_.Get());
}

TEST(AtomicBooleanTest, SetOnNewThread) {
  AtomicBoolean atomic_boolean_{false};

  EXPECT_EQ(false, atomic_boolean_.Get());

  thread::Fiber f([&] { atomic_boolean_.Set(true); });
  f.Join();

  EXPECT_EQ(true, atomic_boolean_.Get());
}

TEST(AtomicBooleanTest, GetOnNewThread) {
  AtomicBoolean atomic_boolean_{false};

  EXPECT_EQ(false, atomic_boolean_.Get());

  atomic_boolean_.Set(true);
  EXPECT_EQ(true, atomic_boolean_.Get());

  thread::Fiber f([&] { EXPECT_EQ(true, atomic_boolean_.Get()); });
  f.Join();
}

}  // namespace
}  // namespace apple
}  // namespace nearby
