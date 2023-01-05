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

#include "internal/platform/implementation/windows/condition_variable.h"

#include <future> // NOLINT

#include "absl/time/clock.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/mutex.h"

#include "gtest/gtest.h"

class ConditionVariableTests : public testing::Test {
 public:
  class ConditionVariableTest {
   public:
    ConditionVariableTest() {}

    std::future<bool> WaitForEvent(bool timedWait, // NOLINT
                                   const absl::Duration* timeout) {
      return std::async(
          std::launch::async,
          [this, timedWait, timeout]() mutable {
            std::thread::id currentThread = std::this_thread::get_id();

            if (timedWait == true) {
              auto result = this->condition_variable_actual_.Wait(*timeout);
              if (result.value == nearby::Exception::kSuccess) {
                return true;
              } else {
                return false;
              }
            } else {
              this->condition_variable_actual_.Wait();
            }
            return true;
          });
    }

    void PostEvent() {
      absl::MutexLock::MutexLock(&mutex_actual_.GetMutex());
      condition_variable_actual_.Notify();
    }

   private:
    nearby::windows::Mutex mutex_actual_ =
        nearby::windows::Mutex(nearby::windows::Mutex::Mode::kRegular);
    nearby::windows::Mutex& mutex_ = mutex_actual_;
    nearby::windows::ConditionVariable condition_variable_actual_ =
        nearby::windows::ConditionVariable(&mutex_);
    nearby::windows::ConditionVariable& condition_variable_ =
        condition_variable_actual_;
  };
};

TEST_F(ConditionVariableTests, SuccessfulCreation) {
  // Arrange
  ConditionVariableTest conditionVariableTest;

  auto result = conditionVariableTest.WaitForEvent(false, nullptr);

  Sleep(1);

  // Act
  conditionVariableTest.PostEvent();

  // Assert
  ASSERT_TRUE(result.get());
}

TEST_F(ConditionVariableTests, TimedCreation) {
  // Arrange
  ConditionVariableTest conditionVariableTest;
  const absl::Duration duration = absl::Milliseconds(100);

  // Act
  auto result = conditionVariableTest.WaitForEvent(true, &duration);

  // Assert
  ASSERT_FALSE(result.get());  // Timed out

  // Act
  result = conditionVariableTest.WaitForEvent(true, &duration);

  Sleep(1);

  conditionVariableTest.PostEvent();

  // Assert
  ASSERT_TRUE(result.get());  // Didn't timeout
}
