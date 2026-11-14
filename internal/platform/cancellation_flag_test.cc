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

#include "internal/platform/cancellation_flag.h"

#include <atomic>
#include <memory>
#include <optional>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {

class CancellationFlagPeer {
 public:
  explicit CancellationFlagPeer(CancellationFlag* cancellation_flag)
      : cancellation_flag_(cancellation_flag) {}
  void RegisterOnCancelListener(CancellationFlag::CancelListener* listener) {
    cancellation_flag_->RegisterOnCancelListener(listener);
  }
  void UnregisterOnCancelListener(CancellationFlag::CancelListener* listener) {
    cancellation_flag_->UnregisterOnCancelListener(listener);
  }
  int CancelListenersSize() const {
    return cancellation_flag_->CancelListenersSize();
  }
  bool WaitForListenersEmpty(absl::Duration timeout) const {
    absl::MutexLock lock(*cancellation_flag_->mutex_);
    auto is_empty =
        [this]() ABSL_SHARED_LOCKS_REQUIRED(*cancellation_flag_->mutex_) {
          return cancellation_flag_->listeners_.empty();
        };
    return cancellation_flag_->mutex_->AwaitWithTimeout(
        absl::Condition(&is_empty), timeout);
  }
  bool WaitForCancelled(absl::Duration timeout) const {
    absl::MutexLock lock(*cancellation_flag_->mutex_);
    auto is_cancelled =
        [this]() ABSL_SHARED_LOCKS_REQUIRED(*cancellation_flag_->mutex_) {
          return cancellation_flag_->cancelled_;
        };
    return cancellation_flag_->mutex_->AwaitWithTimeout(
        absl::Condition(&is_cancelled), timeout);
  }

 private:
  CancellationFlag* cancellation_flag_;  // Not owned by CancellationFlagPeer.
};

namespace {

using FeatureFlags = FeatureFlags::Flags;
using ::testing::MockFunction;
using ::testing::StrictMock;

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{
        .enable_cancellation_flag = true,
    },
    FeatureFlags{
        .enable_cancellation_flag = false,
    },
};

class CancellationFlagTest : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  void SetUp() override {
    feature_flags_ = GetParam();
    env_.SetFeatureFlags(feature_flags_);
  }

  FeatureFlags feature_flags_;
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(CancellationFlagTest, InitialValueIsFalse) {
  CancellationFlag flag;

  // No matter FeatureFlag is enabled or not, Cancelled is always false.
  EXPECT_FALSE(flag.Cancelled());
}

TEST_P(CancellationFlagTest, InitialValueAsTrue) {
  CancellationFlag flag{true};

  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags_.enable_cancellation_flag) {
    EXPECT_FALSE(flag.Cancelled());
    return;
  }

  EXPECT_TRUE(flag.Cancelled());
}

TEST_P(CancellationFlagTest, CanCancel) {
  StrictMock<MockFunction<void()>> mock_cancel_callback;
  CancellationFlag::CancelListener cancel_callback =
      mock_cancel_callback.AsStdFunction();
  EXPECT_CALL(mock_cancel_callback, Call)
      .Times(feature_flags_.enable_cancellation_flag ? 1 : 0);
  CancellationFlag flag;
  CancellationFlagListener cancellation_flag_listener(
      &flag, std::move(cancel_callback));
  flag.Cancel();

  // If FeatureFlag is disabled, return as no-op immediately and
  // Cancelled is always false.
  if (!feature_flags_.enable_cancellation_flag) {
    EXPECT_FALSE(flag.Cancelled());
    return;
  }

  EXPECT_TRUE(flag.Cancelled());
}

TEST_P(CancellationFlagTest, ShouldOnlyCancelOnce) {
  StrictMock<MockFunction<void()>> mock_cancel_callback;
  CancellationFlag::CancelListener cancel_callback =
      mock_cancel_callback.AsStdFunction();
  EXPECT_CALL(mock_cancel_callback, Call)
      .Times(feature_flags_.enable_cancellation_flag ? 1 : 0);

  CancellationFlag flag;
  CancellationFlagListener cancellation_flag_listener(
      &flag, std::move(cancel_callback));
  flag.Cancel();
  flag.Cancel();
  flag.Cancel();

  // If FeatureFlag is disabled, return as no-op immediately and
  // Cancelled is always false.
  if (!feature_flags_.enable_cancellation_flag) {
    EXPECT_FALSE(flag.Cancelled());
    return;
  }

  EXPECT_TRUE(flag.Cancelled());
}

TEST_P(CancellationFlagTest, CannotCancelAfterUnregister) {
  StrictMock<MockFunction<void()>> mock_cancel_callback;
  CancellationFlag::CancelListener cancel_callback =
      mock_cancel_callback.AsStdFunction();
  EXPECT_CALL(mock_cancel_callback, Call).Times(0);

  CancellationFlag flag;
  auto cancellation_flag_listener = std::make_unique<CancellationFlagListener>(
      &flag, std::move(cancel_callback));
  // Release immediately.
  cancellation_flag_listener.reset();
  flag.Cancel();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedCancellationFlagTest, CancellationFlagTest,
                         ::testing::ValuesIn(kTestCases));

class CancellationFlagEnabledTest : public testing::Test {
 protected:
  void SetUp() override {
    original_feature_flags_ = nearby::FeatureFlags::GetInstance().GetFlags();
    env_.SetFeatureFlags(FeatureFlags{
        .enable_cancellation_flag = true,
    });
  }

  void TearDown() override {
    env_.SetFeatureFlags(original_feature_flags_);
  }

  FeatureFlags original_feature_flags_;
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(CancellationFlagEnabledTest, ListenerCanQueryCancelledDuringCancel) {
  CancellationFlag flag;
  bool cancelled_inside_listener = false;
  CancellationFlagListener cancellation_flag_listener(
      &flag, [&flag, &cancelled_inside_listener]() {
        cancelled_inside_listener = flag.Cancelled();
        flag.Cancel();
      });

  flag.Cancel();

  EXPECT_TRUE(cancelled_inside_listener);
}

TEST_F(CancellationFlagEnabledTest,
       ReentrantUncancelAndCancelDuringCancelDoesNotDeadlock) {
  CancellationFlag flag;
  int call_count = 0;
  bool cancelled_after_reentrant_cancel = false;
  CancellationFlagListener listener(&flag, [&]() {
    ++call_count;
    if (call_count == 1) {
      flag.Uncancel();
      flag.Cancel();
      cancelled_after_reentrant_cancel = flag.Cancelled();
    }
  });

  flag.Cancel();

  EXPECT_EQ(2, call_count);
  EXPECT_TRUE(cancelled_after_reentrant_cancel);
  EXPECT_TRUE(flag.Cancelled());
}

TEST_F(CancellationFlagEnabledTest,
       UnregisteredSiblingListenerIsNotInvokedDuringCancel) {
  CancellationFlag flag;
  int call_count = 0;
  std::unique_ptr<CancellationFlagListener> listener_1;
  std::unique_ptr<CancellationFlagListener> listener_2;
  listener_1 = std::make_unique<CancellationFlagListener>(
      &flag, [&listener_2, &call_count]() {
        ++call_count;
        listener_2.reset();
      });
  listener_2 = std::make_unique<CancellationFlagListener>(
      &flag, [&listener_1, &call_count]() {
        ++call_count;
        listener_1.reset();
      });

  flag.Cancel();

  EXPECT_EQ(1, call_count);
}

TEST_F(CancellationFlagEnabledTest,
       UnregisterFromAnotherThreadWaitsForActiveListenerToFinish) {
  CancellationFlag flag;
  CancellationFlagPeer flag_peer(&flag);
  absl::Notification listener_started;
  absl::Notification allow_listener_to_finish;
  absl::Notification unregister_finished;
  std::atomic<bool> listener_finished = false;
  std::atomic<bool> listener_finished_before_unregister_returned = false;

  auto listener = std::make_unique<CancellationFlagListener>(&flag, [&]() {
    listener_started.Notify();
    allow_listener_to_finish.WaitForNotification();
    listener_finished.store(true, std::memory_order_release);
  });

  // Use SingleThreadExecutor because `<thread>` is disallowed in Google3 C++
  // (go/cstyle#Disallowed_Stdlib / cpplint build/c++11).
  SingleThreadExecutor cancel_executor;
  SingleThreadExecutor unregister_executor;
  cancel_executor.Execute([&flag]() { flag.Cancel(); });

  listener_started.WaitForNotification();
  unregister_executor.Execute([&]() {
    listener.reset();
    listener_finished_before_unregister_returned.store(
        listener_finished.load(std::memory_order_acquire),
        std::memory_order_release);
    unregister_finished.Notify();
  });

  // Wait until `unregister_executor` has acquired `*flag.mutex_`, erased
  // `listener` from `listeners_`, and atomically unlocked `*flag.mutex_` to
  // wait inside `mutex_->Await(is_listener_inactive)`.
  EXPECT_TRUE(flag_peer.WaitForListenersEmpty(absl::Seconds(5)));
  allow_listener_to_finish.Notify();
  unregister_finished.WaitForNotification();
  EXPECT_TRUE(listener_finished_before_unregister_returned.load(
      std::memory_order_acquire));
}

TEST_F(CancellationFlagEnabledTest,
       ReRegisteredListenerAddressDuringCancelIsNotInvokedInSamePass) {
  CancellationFlag flag;
  int call_count = 0;
  std::optional<CancellationFlagListener> listener_1;
  std::optional<CancellationFlagListener> listener_2;

  listener_1.emplace(&flag, [&]() {
    ++call_count;
    listener_2.reset();
    listener_2.emplace(&flag, [&call_count]() { ++call_count; });
  });
  listener_2.emplace(&flag, [&]() {
    ++call_count;
    listener_1.reset();
    listener_1.emplace(&flag, [&call_count]() { ++call_count; });
  });

  flag.Cancel();

  EXPECT_EQ(1, call_count);
}

TEST_F(CancellationFlagEnabledTest,
       UncancelDuringCancelStopsRemainingListeners) {
  CancellationFlag flag;
  int call_count = 0;
  CancellationFlagListener listener_1(&flag, [&flag, &call_count]() {
    ++call_count;
    flag.Uncancel();
  });
  CancellationFlagListener listener_2(&flag, [&flag, &call_count]() {
    ++call_count;
    flag.Uncancel();
  });

  flag.Cancel();

  EXPECT_EQ(1, call_count);
  EXPECT_FALSE(flag.Cancelled());
}

TEST_F(CancellationFlagEnabledTest,
       CancelAfterUncancelWaitsForInFlightCancelToFinish) {
  CancellationFlag flag;
  CancellationFlagPeer flag_peer(&flag);
  std::atomic<int> invocation_count = 0;
  absl::Notification first_invocation_started;
  absl::Notification allow_first_invocation_to_finish;
  absl::Notification second_invocation_finished;
  absl::Notification second_cancel_finished;
  std::atomic<bool> first_invocation_finished = false;
  std::atomic<bool> first_finished_before_second_started = false;

  CancellationFlagListener listener(&flag, [&]() {
    if (invocation_count.fetch_add(1, std::memory_order_acq_rel) == 0) {
      first_invocation_started.Notify();
      allow_first_invocation_to_finish.WaitForNotification();
      first_invocation_finished.store(true, std::memory_order_release);
    } else {
      first_finished_before_second_started.store(
          first_invocation_finished.load(std::memory_order_acquire),
          std::memory_order_release);
      second_invocation_finished.Notify();
    }
  });

  // Use SingleThreadExecutor because `<thread>` is disallowed in Google3 C++
  // (go/cstyle#Disallowed_Stdlib / cpplint build/c++11).
  SingleThreadExecutor cancel_executor_1;
  SingleThreadExecutor cancel_executor_2;
  cancel_executor_1.Execute([&flag]() { flag.Cancel(); });

  first_invocation_started.WaitForNotification();
  flag.Uncancel();
  cancel_executor_2.Execute([&]() {
    flag.Cancel();
    second_cancel_finished.Notify();
  });

  // Wait until `cancel_executor_2` has acquired `*flag.mutex_`, set
  // `cancelled_ = true` and `pending_cancel_listeners_ = listeners_`, and
  // atomically unlocked `*flag.mutex_` to wait inside `mutex_->Await`.
  EXPECT_TRUE(flag_peer.WaitForCancelled(absl::Seconds(5)));
  allow_first_invocation_to_finish.Notify();
  second_invocation_finished.WaitForNotification();
  second_cancel_finished.WaitForNotification();
  EXPECT_TRUE(
      first_finished_before_second_started.load(std::memory_order_acquire));
  EXPECT_EQ(2, invocation_count.load(std::memory_order_acquire));
}

TEST_F(CancellationFlagEnabledTest,
       ConcurrentCancelsAfterUncancelDoNotTriggerSpuriousThirdPass) {
  CancellationFlag flag;
  CancellationFlagPeer flag_peer(&flag);
  std::atomic<int> invocation_count = 0;
  absl::Notification first_invocation_started;
  absl::Notification allow_first_invocation_to_finish;
  absl::Notification cancel_1_finished;
  absl::Notification cancel_2_finished;

  CancellationFlagListener listener(&flag, [&]() {
    const int pass =
        invocation_count.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (pass == 1) {
      first_invocation_started.Notify();
      allow_first_invocation_to_finish.WaitForNotification();
    } else if (pass == 2) {
      flag.Uncancel();
    }
  });

  // Use SingleThreadExecutor because `<thread>` is disallowed in Google3 C++
  // (go/cstyle#Disallowed_Stdlib / cpplint build/c++11).
  SingleThreadExecutor cancel_executor_1;
  SingleThreadExecutor cancel_executor_2;
  cancel_executor_1.Execute([&]() {
    flag.Cancel();
    cancel_1_finished.Notify();
  });

  first_invocation_started.WaitForNotification();
  flag.Uncancel();
  EXPECT_FALSE(flag.Cancelled());

  cancel_executor_2.Execute([&]() {
    flag.Cancel();
    cancel_2_finished.Notify();
  });

  // Wait until `cancel_executor_2` has entered `Cancel()`, set
  // `cancelled_ = true` and repopulated `pending_cancel_listeners_`, and
  // blocked inside `mutex_->Await(is_not_in_cancel)`.
  EXPECT_TRUE(flag_peer.WaitForCancelled(absl::Seconds(5)));
  EXPECT_TRUE(flag.Cancelled());

  allow_first_invocation_to_finish.Notify();
  cancel_1_finished.WaitForNotification();
  cancel_2_finished.WaitForNotification();

  EXPECT_EQ(2, invocation_count.load(std::memory_order_acquire));
  EXPECT_FALSE(flag.Cancelled());
}

TEST_F(CancellationFlagEnabledTest,
       UnregisterNullListenerDoesNotAffectRegisteredListeners) {
  CancellationFlag flag;
  CancellationFlagPeer flag_peer(&flag);
  bool listener_invoked = false;
  CancellationFlagListener listener(
      &flag, [&listener_invoked]() { listener_invoked = true; });

  flag_peer.UnregisterOnCancelListener(nullptr);
  flag.Cancel();

  EXPECT_TRUE(listener_invoked);
}

}  // namespace

TEST(CancellationFlagTest,
     CancelMultiplesIfMultiplePointersToTheSameFunctionRegistered) {
  nearby::FeatureFlags::Flags feature_flags_ = nearby::FeatureFlags::Flags{
      .enable_cancellation_flag = true,
  };
  MediumEnvironment::Instance().SetFeatureFlags(feature_flags_);

  StrictMock<MockFunction<void()>> mock_cancel_callback;
  CancellationFlag::CancelListener cancel_callback =
      mock_cancel_callback.AsStdFunction();
  CancellationFlag::CancelListener cancel_callback_copy =
      mock_cancel_callback.AsStdFunction();

  CancellationFlag::CancelListener* callback_pointer_1 = &cancel_callback;
  auto callback_pointer_2 = std::make_unique<CancellationFlag::CancelListener>(
      std::move(cancel_callback_copy));

  EXPECT_NE(callback_pointer_1, callback_pointer_2.get());
  EXPECT_CALL(mock_cancel_callback, Call).Times(2);

  CancellationFlag flag;
  CancellationFlagPeer flag_peer(&flag);
  flag_peer.RegisterOnCancelListener(callback_pointer_1);
  flag_peer.RegisterOnCancelListener(callback_pointer_2.get());

  flag.Cancel();

  flag_peer.UnregisterOnCancelListener(callback_pointer_2.get());
  EXPECT_EQ(1, flag_peer.CancelListenersSize());
  flag_peer.UnregisterOnCancelListener(callback_pointer_1);
  EXPECT_EQ(0, flag_peer.CancelListenersSize());
}

TEST(CancellationFlagTest, RegisteredMultuipleTimesOnlyCancelOnce) {
  nearby::FeatureFlags::Flags feature_flags_ = nearby::FeatureFlags::Flags{
      .enable_cancellation_flag = true,
  };
  MediumEnvironment::Instance().SetFeatureFlags(feature_flags_);

  StrictMock<MockFunction<void()>> mock_cancel_callback;
  CancellationFlag::CancelListener cancel_callback =
      mock_cancel_callback.AsStdFunction();
  EXPECT_CALL(mock_cancel_callback, Call).Times(1);

  CancellationFlag flag;
  CancellationFlagPeer flag_peer(&flag);
  flag_peer.RegisterOnCancelListener(&cancel_callback);
  flag_peer.RegisterOnCancelListener(&cancel_callback);
  EXPECT_EQ(1, flag_peer.CancelListenersSize());

  flag.Cancel();

  flag_peer.UnregisterOnCancelListener(&cancel_callback);
  EXPECT_EQ(0, flag_peer.CancelListenersSize());
  flag_peer.UnregisterOnCancelListener(&cancel_callback);
}

}  // namespace nearby
