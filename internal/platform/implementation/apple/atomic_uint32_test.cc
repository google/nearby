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

#include "internal/platform/implementation/apple/atomic_uint32.h"

#include "gtest/gtest.h"
#include "thread/fiber/fiber.h"

namespace nearby {
namespace apple {
namespace {

TEST(AtomicUint32Test, GetOnSameThread) {
  std::uint32_t initial_value = 1450;
  AtomicUint32 atomic_reference_{initial_value};

  EXPECT_EQ(initial_value, atomic_reference_.Get());
}

TEST(AtomicUint32Test, SetGetOnSameThread) {
  std::uint32_t initial_value_ = 1450;
  AtomicUint32 atomic_reference_{initial_value_};

  std::uint32_t new_value = 28;
  atomic_reference_.Set(new_value);
  EXPECT_EQ(new_value, atomic_reference_.Get());
}

TEST(AtomicUint32Test, SetOnNewThread) {
  std::uint32_t initial_value_ = 1450;
  AtomicUint32 atomic_reference_{initial_value_};

  std::uint32_t new_thread_value = 28;
  thread::Fiber f([&] { atomic_reference_.Set(new_thread_value); });
  f.Join();
  EXPECT_EQ(new_thread_value, atomic_reference_.Get());
}

TEST(AtomicUint32Test, GetOnNewThread) {
  std::uint32_t initial_value_ = 1450;
  AtomicUint32 atomic_reference_{initial_value_};

  std::uint32_t new_value = 28;
  atomic_reference_.Set(new_value);
  thread::Fiber f([&] { EXPECT_EQ(new_value, atomic_reference_.Get()); });
  f.Join();
}

}  // namespace
}  // namespace apple
}  // namespace nearby
