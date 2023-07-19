// Copyright 2021 Google LLC
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

#include "internal/test/fake_clock.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace {

TEST(FakeClock, TestGetCurrentTime) {
  FakeClock clock;
  EXPECT_GT(absl::ToUnixNanos(clock.Now()), 0);
}

TEST(FakeClock, TestFastForward) {
  FakeClock clock;
  absl::Time time1 = clock.Now();
  clock.FastForward(absl::Nanoseconds(1500));
  absl::Time time2 = clock.Now();
  EXPECT_EQ(absl::ToUnixNanos(time1) + 1500, absl::ToUnixNanos(time2));
  EXPECT_EQ(clock.GetObserversCount(), 0);
}

TEST(FakeClock, TestReset) {
  FakeClock clock;
  int count = 0;
  auto observer = [&count]() { count++; };
  clock.AddObserver("test", observer);
  clock.FastForward(absl::Nanoseconds(1500));
  EXPECT_EQ(count, 1);
  EXPECT_EQ(clock.GetObserversCount(), 1);
  clock.Reset();
  EXPECT_EQ(clock.GetObserversCount(), 0);
}

TEST(FakeClock, TestObserver) {
  FakeClock clock;
  int count = 0;
  auto observer = [&count]() { count++; };
  clock.AddObserver("test", observer);
  clock.FastForward(absl::Nanoseconds(1500));
  EXPECT_EQ(count, 1);
  clock.FastForward(absl::Nanoseconds(1500));
  EXPECT_EQ(count, 2);
  clock.RemoveObserver("test");
  clock.FastForward(absl::Nanoseconds(1500));
  EXPECT_EQ(count, 2);
}

}  // namespace
}  // namespace nearby
