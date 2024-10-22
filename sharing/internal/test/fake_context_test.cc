// Copyright 2021-2023 Google LLC
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

#include "sharing/internal/test/fake_context.h"

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/task_runner.h"

namespace nearby {
namespace {

TEST(FakeContext, TestAccessMockContext) {
  FakeContext context;
  EXPECT_NE(context.GetClock(), nullptr);
  EXPECT_NE(context.CreateTimer(), nullptr);
  EXPECT_NE(context.GetConnectivityManager(), nullptr);
  EXPECT_NE(context.CreateSequencedTaskRunner(), nullptr);
  EXPECT_NE(context.CreateConcurrentTaskRunner(5), nullptr);
}

TEST(FakeContext, ExecuteTask) {
  FakeContext context;
  absl::Notification notification;
  bool is_called = false;
  context.GetTaskRunner()->PostTask([&]() {
    is_called = true;
    notification.Notify();
  });

  EXPECT_TRUE(notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_TRUE(is_called);
}

}  // namespace
}  // namespace nearby
