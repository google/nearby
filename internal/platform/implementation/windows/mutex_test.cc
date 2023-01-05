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

#include "internal/platform/implementation/windows/mutex.h"

#include <future>  // NOLINT

#include "gtest/gtest.h"

class MutexTests : public testing::Test {
 public:
  class MutexTest {
   public:
    MutexTest(nearby::windows::Mutex& mutex) : mutex_(mutex) {}

    std::future<bool> WaitForLock() {  // NOLINT
      return std::async(std::launch::async,
                        // for this lambda you need C++14
                        [this]() mutable {
                          absl::MutexLock::MutexLock(&mutex_.GetMutex());
                          return true;
                        });
    }

    void PostEvent() {
      absl::MutexLock::MutexLock(&mutex_.GetMutex());
      mutex_.Unlock();
    }

   private:
    nearby::windows::Mutex& mutex_;
  };
};

TEST_F(MutexTests, SuccessfulRecursiveCreation) {
  // Arrange
  nearby::windows::Mutex mutex =
      nearby::windows::Mutex(nearby::windows::Mutex::Mode::kRecursive);

  // Act
  std::recursive_mutex& actual = mutex.GetRecursiveMutex();

  // Assert
  ASSERT_TRUE(actual.native_handle() != nullptr);
}

TEST_F(MutexTests, SuccessfulCreation) {
  // Arrange
  nearby::windows::Mutex mutex(nearby::windows::Mutex::Mode::kRegular);

  // Act
  absl::Mutex& actual = mutex.GetMutex();

  // Assert
  ASSERT_TRUE(&actual != nullptr);
}

TEST_F(MutexTests, SuccessfulSignal) {
  // Arrange
  nearby::windows::Mutex mutex(nearby::windows::Mutex::Mode::kRegular);

  nearby::windows::Mutex& mutexRef = mutex;
  MutexTest mutexTest(mutexRef);

  mutex.Lock();

  // Act
  auto result = mutexTest.WaitForLock();
  mutex.Unlock();

  // Assert
  ASSERT_TRUE(result.get());
}

TEST_F(MutexTests, SuccessfulRecursiveSignal) {
  // Arrange
  nearby::windows::Mutex mutex(nearby::windows::Mutex::Mode::kRecursive);

  nearby::windows::Mutex& mutexRef = mutex;
  MutexTest mutexTest(mutexRef);

  mutex.Lock();
  mutex.Lock();
  mutex.Lock();

  // Act
  auto result = mutexTest.WaitForLock();
  mutex.Unlock();
  mutex.Unlock();
  mutex.Unlock();

  // Assert
  ASSERT_TRUE(result.get());
}
