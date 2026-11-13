// Copyright 2026 Google LLC
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

#include "internal/test/test_single_thread_executor.h"

#include <memory>

#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/timer.h"
#include "internal/test/fake_timer.h"

namespace nearby {

TestSingleThreadExecutor::~TestSingleThreadExecutor() { Shutdown(); }

void TestSingleThreadExecutor::Shutdown() {
  closed_ = true;
  SingleThreadExecutor::Shutdown();
}

void TestSingleThreadExecutor::Sync() {
  if (closed_) {
    return;
  }
  absl::Notification notification;
  Execute([&notification] { notification.Notify(); });
  notification.WaitForNotification();
}

bool TestSingleThreadExecutor::SyncWithTimeout(absl::Duration timeout) {
  if (closed_) {
    return true;
  }
  absl::Notification notification;
  Execute([&notification] { notification.Notify(); });
  return notification.WaitForNotificationWithTimeout(timeout);
}

std::unique_ptr<Timer> TestSingleThreadExecutor::CreateTimer() {
  return std::make_unique<FakeTimer>(clock_);
}

}  // namespace nearby
