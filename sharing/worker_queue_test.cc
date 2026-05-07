// Copyright 2024 Google LLC
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

#include "sharing/worker_queue.h"

#include <memory>
#include <queue>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"

namespace nearby::sharing {
namespace {

TEST(WorkerQueueTest, SecondStartReturnsFalse) {
  FakeClock fake_clock;
  FakeTaskRunner task_runner(&fake_clock, 1);
  WorkerQueue<int> queue(&task_runner);
  EXPECT_TRUE(queue.Start([]() {}));
  EXPECT_FALSE(queue.Start([]() {}));
}

TEST(WorkerQueueTest, StartAfterStopReturnsFalse) {
  FakeClock fake_clock;
  FakeTaskRunner task_runner(&fake_clock, 1);
  WorkerQueue<int> queue(&task_runner);
  EXPECT_TRUE(queue.Start([]() {}));
  queue.Stop();
  EXPECT_FALSE(queue.Start([]() {}));
}

TEST(WorkerQueueTest, QueueItems) {
  FakeClock fake_clock;
  FakeTaskRunner task_runner(&fake_clock, 1);
  WorkerQueue<int> queue(&task_runner);
  absl::Notification notification;
  // Block the task runner thread.
  task_runner.PostTask(
      [&notification]() { notification.WaitForNotification(); });
  EXPECT_TRUE(queue.Start([&queue]() {
    std::queue<int> items = queue.ReadAll();
    EXPECT_EQ(items.size(), 2);
    EXPECT_EQ(items.front(), 1);
    EXPECT_EQ(items.back(), 2);
  }));
  queue.Queue(1);
  queue.Queue(2);
  notification.Notify();

  task_runner.Sync();
  // No more callbacks.
}

TEST(WorkerQueueTest, QueueItemsWhileCallbackRunning) {
  FakeClock fake_clock;
  FakeTaskRunner task_runner(&fake_clock, 1);
  WorkerQueue<int> queue(&task_runner);
  absl::Notification notification1;
  absl::Notification notification2;
  EXPECT_TRUE(queue.Start([&queue, &notification1, &notification2]() {
    notification1.Notify();
    // Block the task runner thread.
    notification2.WaitForNotification();
    std::queue<int> items = queue.ReadAll();
    EXPECT_EQ(items.size(), 2);
    EXPECT_EQ(items.front(), 1);
    EXPECT_EQ(items.back(), 2);
  }));
  queue.Queue(1);
  // Wait for the callback to start.
  notification1.WaitForNotification();
  queue.Queue(2);
  notification2.Notify();

  task_runner.Sync();
  // No more callbacks.
}

TEST(WorkerQueueTest, StopStopsCallback) {
  FakeClock fake_clock;
  FakeTaskRunner task_runner(&fake_clock, 1);
  auto queue = std::make_unique<WorkerQueue<int>>(&task_runner);
  queue->Queue(1);
  queue->Queue(2);
  absl::Notification notification;
  auto queue_ptr = queue.get();
  EXPECT_TRUE(queue->Start([queue_ptr, &notification]() {
    std::queue<int> items = queue_ptr->ReadAll();
    EXPECT_EQ(items.size(), 2);
    EXPECT_EQ(items.front(), 1);
    EXPECT_EQ(items.back(), 2);
    notification.Notify();
  }));
  // Wait for the callback to start.
  notification.WaitForNotification();
  queue->Stop();
  queue->Queue(3);
  queue.reset();
  task_runner.Sync();
  // No more callbacks.
}

TEST(WorkerQueueTest, StopWaitsForInFlightCallback) {
  FakeClock fake_clock;
  FakeTaskRunner task_runner(&fake_clock, 1);
  auto queue = std::make_unique<WorkerQueue<int>>(&task_runner);
  absl::Notification notification1;
  absl::Notification notification2;
  auto queue_ptr = queue.get();
  EXPECT_TRUE(queue->Start([queue_ptr, &notification1, &notification2]() {
    std::queue<int> items = queue_ptr->ReadAll();
    notification1.Notify();
    notification2.WaitForNotificationWithTimeout(absl::Milliseconds(500));
    EXPECT_EQ(items.size(), 2);
    EXPECT_EQ(items.front(), 1);
    EXPECT_EQ(items.back(), 2);
  }));
  queue->Queue(1);
  queue->Queue(2);
  // Wait for the callback to start.
  notification1.WaitForNotification();
  queue->Queue(3);
  queue.reset();
  task_runner.Sync();
  // No more callbacks.
}

}  // namespace
}  // namespace nearby::sharing
