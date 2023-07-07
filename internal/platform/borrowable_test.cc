// Copyright 2022 Google LLC
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

#include "internal/platform/borrowable.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {

namespace {

constexpr int kDefaultValue = -1;
class Data {
 public:
  Data() = default;
  Data(const Data &) = delete;
  Data(Data &&) = default;
  void SetValue(int value) { value_ = value; }
  int GetValue() { return value_; }

 private:
  volatile int value_ = kDefaultValue;
};

TEST(Borrowable, CreateAndBorrow) {
  constexpr int kValue = 1;

  Lender<int> lender(kValue);
  Borrowable<int> borrowable = lender.GetBorrowable();
  Borrowed<int> borrowed = borrowable.Borrow();

  ASSERT_TRUE(borrowed);
  EXPECT_EQ(*borrowed, kValue);
}

TEST(Borrowable, BorrowAccessesOriginalObject) {
  constexpr int kValue = 12;
  Lender<Data> lender(Data{});
  Borrowable<Data> borrowable = lender.GetBorrowable();
  {
    Borrowed<Data> borrowed = borrowable.Borrow();

    ASSERT_TRUE(borrowed);
    borrowed->SetValue(kValue);
  }
  EXPECT_EQ(borrowable.Borrow()->GetValue(), kValue);
}

TEST(Borrowable, BorrowAfterReleaseFails) {
  constexpr int kValue = 1;
  Lender<int> lender(kValue);
  Borrowable<int> borrowable = lender.GetBorrowable();

  lender.Release();

  EXPECT_FALSE(borrowable.Borrow());
}

TEST(Borrowable, ReleaseWaitsForBorrowToEnd) {
  constexpr int kValue = 1;
  Data *data = new Data();
  CountDownLatch latch(1);
  Lender<Data *> lender(data);
  Borrowable<Data *> borrowable = lender.GetBorrowable();
  SingleThreadExecutor executor;
  Borrowed<Data *> borrowed = borrowable.Borrow();
  ASSERT_TRUE(borrowed);

  executor.Execute([&]() {
    latch.CountDown();
    // Release() must wait until `borrowed` object goes out of scope.
    lender.Release();
    data->SetValue(kValue);
    delete data;
  });
  EXPECT_TRUE(latch.Await().Ok());
  // If `Release()` didn't wait, then we would access a dead object below. Short
  // sleep makes the crash more likely.
  absl::SleepFor(absl::Milliseconds(100));
  EXPECT_EQ((*borrowed)->GetValue(), kDefaultValue);
}

TEST(Borrowable, BorrowIsExclusive) {
  constexpr int kThreads = 15;
  constexpr int kTasks = 30;
  CountDownLatch latch(kTasks);
  Lender<Data> lender(Data{});
  MultiThreadExecutor executor(kThreads);

  for (int i = 0; i < kTasks; i++) {
    executor.Execute([&, value = i, borrowable = lender.GetBorrowable()]() {
      latch.CountDown();
      Borrowed<Data> borrowed = borrowable.Borrow();
      ASSERT_TRUE(borrowed);

      // Verify that a task has exclusive access to the borrowed object.
      borrowed->SetValue(value);
      // Sleep yields to other threads, which increases the chance of a race
      // condition.
      absl::SleepFor(absl::Milliseconds(1));
      EXPECT_EQ(borrowed->GetValue(), value);
    });
  }
  EXPECT_TRUE(latch.Await().Ok());
  // Verify that the shared data was modified. We can't know for sure what the
  // final value is.
  EXPECT_NE(lender.GetBorrowable().Borrow()->GetValue(), kDefaultValue);
}

TEST(Borrowable, DefaultBorrowableFails) {
  Borrowable<int> borrowable;
  EXPECT_FALSE(borrowable.Borrow());
}

TEST(Borrowable, CopyBorrowable) {
  constexpr int kValue = 1;

  Lender<int> lender(kValue);
  Borrowable<int> borrowable = lender.GetBorrowable();
  Borrowable<int> copy = borrowable;
  Borrowed<int> borrowed = copy.Borrow();

  ASSERT_TRUE(borrowed);
  EXPECT_EQ(*borrowed, kValue);
}

}  // namespace
}  // namespace nearby
