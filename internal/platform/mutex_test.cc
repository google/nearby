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

#include "internal/platform/mutex.h"

#include "gtest/gtest.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace {

class MutexTest : public testing::Test {
 public:
  void VerifyStepReached(int expected) {
    absl::MutexLock lock(&step_mutex_);
    absl::Time deadline = absl::Now() + kTimeToWait;
    while (step_ != expected) {
      if (step_cond_.WaitWithDeadline(&step_mutex_, deadline)) break;
    }
    EXPECT_EQ(step_, expected);
    // Make sure we are not progressing further.
    absl::SleepFor(kTimeToWait);
    EXPECT_EQ(step_, expected);
  }

 protected:
  SingleThreadExecutor executor_;
  const absl::Duration kTimeToWait = absl::Milliseconds(500);
  std::atomic_int step_ = 0;
  absl::Mutex step_mutex_;
  absl::CondVar step_cond_;
};

TEST_F(MutexTest, ConstructorDestructorWorks) {
  Mutex test_mutex;
  SUCCEED();
}

TEST_F(MutexTest, BasicLockingWorks) {
  Mutex test_mutex;
  test_mutex.Lock();
  executor_.Execute([this, &test_mutex]() {
    step_ = 1;
    step_cond_.Signal();
    test_mutex.Lock();
    test_mutex.Unlock();
    step_ = 2;
    step_cond_.Signal();
  });
  VerifyStepReached(1);
  test_mutex.Unlock();
  VerifyStepReached(2);
}

#ifdef THREAD_SANITIZER
TEST_F(MutexTest, DISABLED_DoubleLockIsDeadlock)
ABSL_NO_THREAD_SAFETY_ANALYSIS {
#else
TEST_F(MutexTest, DoubleLockIsDeadlock) ABSL_NO_THREAD_SAFETY_ANALYSIS {
#endif
  Mutex test_mutex{/*check=*/false};  // Disable run-time deadlock detection.
  test_mutex.Lock();
  executor_.Execute([this, &test_mutex]() ABSL_NO_THREAD_SAFETY_ANALYSIS {
    step_ = 1;
    step_cond_.Signal();  // We entered executor.
    test_mutex.Lock();
    step_ = 2;
    step_cond_.Signal();  // We acquired the test lock.
    test_mutex.Lock();    // Deadlock. (Main thread should save us).
    step_ = 3;
    step_cond_.Signal();  // We are done.
  });
  VerifyStepReached(1);
  test_mutex.Unlock();  // Let executor proceed to step 2.
  VerifyStepReached(2);
  test_mutex.Unlock();  // Bring executor out of deadlock.
  VerifyStepReached(3);
  test_mutex.Unlock();  // Unlock before shutdown.
}

TEST_F(MutexTest, DoubleLockIsNotDeadlock) {
  RecursiveMutex test_mutex;
  test_mutex.Lock();
  executor_.Execute([this, &test_mutex]() ABSL_NO_THREAD_SAFETY_ANALYSIS {
    step_ = 1;
    step_cond_.Signal();  // We entered executor.
    test_mutex.Lock();
    test_mutex.Lock();
    test_mutex.Unlock();
    test_mutex.Unlock();
    step_ = 2;
    step_cond_.Signal();  // We are done.
  });
  VerifyStepReached(1);
  test_mutex.Unlock();  // Let executor continue.
  VerifyStepReached(2);
}

}  // namespace
}  // namespace nearby
