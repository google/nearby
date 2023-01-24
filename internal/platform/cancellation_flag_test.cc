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

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/medium_environment.h"

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
