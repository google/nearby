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

#include "internal/platform/implementation/apple/mutex.h"

#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "thread/fiber/fiber.h"

namespace nearby {
namespace apple {
namespace {

static const absl::Duration kTimeToWait = absl::Milliseconds(500);

TEST(MutexTest, LockOnce_UnlockOnce) {
  Mutex test_mutex_1{};
  test_mutex_1.Lock();
  test_mutex_1.Unlock();

  RecursiveMutex test_mutex_2;
  test_mutex_2.Lock();
  test_mutex_2.Unlock();
}

TEST(MutexTest, BasicLockingWorks) {
  absl::Notification lock_obtained;
  Mutex test_mutex{};
  test_mutex.Lock();
  thread::Fiber f([&test_mutex, &lock_obtained] {
    test_mutex.Lock();
    test_mutex.Unlock();
    lock_obtained.Notify();
  });
  EXPECT_FALSE(lock_obtained.WaitForNotificationWithTimeout(kTimeToWait));
  test_mutex.Unlock();
  EXPECT_TRUE(lock_obtained.WaitForNotificationWithTimeout(kTimeToWait));
  f.Join();
}

TEST(MutexTest, RecursiveLockingWorks) {
  absl::Notification lock_obtained;
  RecursiveMutex test_mutex;
  test_mutex.Lock();
  thread::Fiber f([&test_mutex, &lock_obtained] {
    test_mutex.Lock();
    test_mutex.Unlock();
    lock_obtained.Notify();
  });
  EXPECT_FALSE(lock_obtained.WaitForNotificationWithTimeout(kTimeToWait));
  test_mutex.Unlock();
  EXPECT_TRUE(lock_obtained.WaitForNotificationWithTimeout(kTimeToWait));
  f.Join();
}

TEST(MutexTest, RecursiveLockingForNestedWorks) {
  absl::Notification lock_obtained;
  RecursiveMutex test_mutex;
  test_mutex.Lock();
  thread::Fiber f([&test_mutex, &lock_obtained]()
                      ABSL_NO_THREAD_SAFETY_ANALYSIS {
                        test_mutex.Lock();
                        test_mutex.Lock();
                        test_mutex.Unlock();
                        test_mutex.Unlock();
                        lock_obtained.Notify();
                      });
  EXPECT_FALSE(lock_obtained.WaitForNotificationWithTimeout(kTimeToWait));
  test_mutex.Unlock();
  EXPECT_TRUE(lock_obtained.WaitForNotificationWithTimeout(kTimeToWait));
  f.Join();
}

TEST(MutexTest, AssertHeld) {
  Mutex mutex;
  mutex.Lock();
  mutex.AssertHeld();
  mutex.Unlock();
}

TEST(MutexTest, RecursiveAssertHeld) ABSL_NO_THREAD_SAFETY_ANALYSIS {
  RecursiveMutex mutex;
  mutex.Lock();
  mutex.AssertHeld();
  mutex.Lock();
  mutex.AssertHeld();
  mutex.Unlock();
  mutex.AssertHeld();
  mutex.Unlock();
}

#if !defined(NDEBUG) && defined(GTEST_HAS_DEATH_TEST)
TEST(MutexDeathTest, MutexAssertHeldWithoutLock) {
  Mutex mutex;
  EXPECT_DEATH(mutex.AssertHeld(), "");
}

TEST(MutexDeathTest, RecursiveMutexAssertHeldWithoutLock) {
  RecursiveMutex mutex;
  EXPECT_DEATH(mutex.AssertHeld(), "");
}
#endif  // !defined(NDEBUG) && defined(GTEST_HAS_DEATH_TEST)

}  // namespace
}  // namespace apple
}  // namespace nearby
