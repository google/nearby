// Copyright 2023 Google LLC
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

#include "fastpair/testing/fake_fast_pair_context.h"

#include "gtest/gtest.h"

namespace nearby {
namespace fastpair {
namespace {

TEST(FakeContext, TestAccessMockContext) {
  FakeFastPairContext context;
  EXPECT_NE(context.GetPreferencesManager(), nullptr);
  EXPECT_NE(context.GetAuthenticationManager(), nullptr);
  EXPECT_NE(context.GetDeviceInfo(), nullptr);
}

TEST(FakeContext, ExecuteTask) {
  FakeFastPairContext context;
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
}  // namespace fastpair
}  // namespace nearby
