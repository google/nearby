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

#include "sharing/scheduling/nearby_share_scheduler_base.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/scheduling/nearby_share_scheduler.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::api::PreferenceManager;

const char kTestPrefName[] = "test_pref_name";

constexpr absl::Duration kZeroTimeDuration = absl::ZeroDuration();
constexpr absl::Duration kBaseRetryDuration = absl::Seconds(5);
constexpr absl::Duration kMaxRetryDuration = absl::Hours(1);

constexpr absl::Duration kTestTimeUntilRecurringRequest = absl::Minutes(120);

class NearbyShareSchedulerBaseForTest : public NearbyShareSchedulerBase {
 public:
  NearbyShareSchedulerBaseForTest(
      Context* context,
      PreferenceManager& preference_manager,
      std::optional<absl::Duration> time_until_recurring_request,
      bool retry_failures, bool require_connectivity,
      absl::string_view pref_name, OnRequestCallback callback)
      : NearbyShareSchedulerBase(context, preference_manager, retry_failures,
                                 require_connectivity, pref_name,
                                 std::move(callback)),
        time_until_recurring_request_(time_until_recurring_request) {}

  ~NearbyShareSchedulerBaseForTest() override = default;

 private:
  std::optional<absl::Duration> TimeUntilRecurringRequest(
      absl::Time now) const override {
    return time_until_recurring_request_;
  }

  std::optional<absl::Duration> time_until_recurring_request_;
};

class NearbyShareSchedulerBaseTest : public ::testing::Test {
 protected:
  NearbyShareSchedulerBaseTest() = default;

  ~NearbyShareSchedulerBaseTest() override = default;

  void SetUp() override {
    preference_manager_.Remove(kTestPrefName);
  }

  void CreateScheduler(
      bool retry_failures, bool require_connectivity,
      std::optional<absl::Duration> time_until_recurring_request =
          kTestTimeUntilRecurringRequest) {
    scheduler_ = std::make_unique<NearbyShareSchedulerBaseForTest>(
        &fake_context_, preference_manager_, time_until_recurring_request,
        retry_failures, require_connectivity, kTestPrefName, callback_);
  }

  void DestroyScheduler() { scheduler_.reset(); }

  void StartScheduling() {
    scheduler_->Start();
    EXPECT_TRUE(scheduler_->is_running());
  }

  void StopScheduling() {
    scheduler_->Stop();
    EXPECT_FALSE(scheduler_->is_running());
  }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(absl::Duration delta) {
    fake_context_.fake_clock()->FastForward(delta);
  }

  void RunPendingRequest() {
    EXPECT_FALSE(scheduler_->IsWaitingForResult());
    std::optional<absl::Duration> time_until_next_request =
        scheduler_->GetTimeUntilNextRequest();
    ASSERT_TRUE(time_until_next_request);
    FastForward(*time_until_next_request);
  }

  void FinishPendingRequest(bool success) {
    EXPECT_TRUE(scheduler_->IsWaitingForResult());
    EXPECT_FALSE(scheduler_->GetTimeUntilNextRequest().has_value());
    size_t num_failures = scheduler_->GetNumConsecutiveFailures();
    std::optional<absl::Time> last_success_time =
        scheduler_->GetLastSuccessTime();
    scheduler_->HandleResult(success);
    EXPECT_FALSE(scheduler_->IsWaitingForResult());
    EXPECT_EQ(scheduler_->GetNumConsecutiveFailures(),
              success ? 0 : num_failures + 1);
    EXPECT_EQ(
        scheduler_->GetLastSuccessTime(),
        success ? std::make_optional<absl::Time>(Now()) : last_success_time);
  }

  absl::Time Now() const {
    return fake_context_.GetClock()->Now();
  }

  size_t on_request_call_count() const { return on_request_call_count_; }
  NearbyShareScheduler* scheduler() { return scheduler_.get(); }

 private:
  nearby::FakePreferenceManager preference_manager_;
  nearby::FakeContext fake_context_;
  size_t on_request_call_count_ = 0;
  std::unique_ptr<NearbyShareScheduler> scheduler_;
  NearbyShareScheduler::OnRequestCallback callback_ = [&]() {
    ++on_request_call_count_;
  };
};

TEST_F(NearbyShareSchedulerBaseTest, ImmediateRequest) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  scheduler()->MakeImmediateRequest();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), kZeroTimeDuration);
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), 1u);
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbyShareSchedulerBaseTest, RecurringRequest) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(),
            kTestTimeUntilRecurringRequest);

  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  FinishPendingRequest(/*success=*/true);
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(),
            kTestTimeUntilRecurringRequest);
}

TEST_F(NearbyShareSchedulerBaseTest, NoRecurringRequest) {
  // The flavor of the schedule does not schedule recurring requests.
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true,
                  /*time_until_recurring_request=*/std::nullopt);
  StartScheduling();
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());

  scheduler()->MakeImmediateRequest();
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  FinishPendingRequest(/*success=*/true);

  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());
}

TEST_F(NearbyShareSchedulerBaseTest, SchedulingNotStarted) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  EXPECT_FALSE(scheduler()->is_running());
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());
  EXPECT_FALSE(scheduler()->IsWaitingForResult());

  // Request remains pending until scheduling starts.
  scheduler()->MakeImmediateRequest();
  EXPECT_FALSE(scheduler()->is_running());
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
  StartScheduling();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), kZeroTimeDuration);
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
}

TEST_F(NearbyShareSchedulerBaseTest, DoNotRetryFailures) {
  CreateScheduler(/*retry_failures=*/false, /*require_connectivity=*/true);
  StartScheduling();

  // Run recurring request.
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), 1u);
  FinishPendingRequest(/*success=*/false);
  EXPECT_EQ(scheduler()->GetNumConsecutiveFailures(), 1u);

  // Failure is not automatically retried; the recurring request is re-scheduled
  // instead.
  EXPECT_EQ(kTestTimeUntilRecurringRequest,
            scheduler()->GetTimeUntilNextRequest());

  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), 2u);
  FinishPendingRequest(/*success=*/false);
  EXPECT_EQ(scheduler()->GetNumConsecutiveFailures(), 2u);
}

TEST_F(NearbyShareSchedulerBaseTest, FailureRetry) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  scheduler()->MakeImmediateRequest();

  size_t num_failures = 0;
  size_t expected_backoff_factor = 1;
  do {
    EXPECT_EQ(scheduler()->GetNumConsecutiveFailures(), num_failures);
    ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
    EXPECT_EQ(on_request_call_count(), num_failures + 1);
    FinishPendingRequest(/*success=*/false);
    EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(),
              std::min(kMaxRetryDuration,
                       kBaseRetryDuration * expected_backoff_factor));
    expected_backoff_factor *= 2;
    ++num_failures;
  } while (*scheduler()->GetTimeUntilNextRequest() != kMaxRetryDuration);

  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), num_failures + 1);
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbyShareSchedulerBaseTest,
       FailureRetry_InterruptWithImmediateAttempt) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  scheduler()->MakeImmediateRequest();

  size_t num_failures = 0;
  size_t expected_backoff_factor = 1;
  do {
    EXPECT_EQ(scheduler()->GetNumConsecutiveFailures(), num_failures);
    ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
    EXPECT_EQ(on_request_call_count(), num_failures + 1);
    FinishPendingRequest(/*success=*/false);
    EXPECT_EQ(std::min(kMaxRetryDuration,
                       kBaseRetryDuration * expected_backoff_factor),
              scheduler()->GetTimeUntilNextRequest());
    expected_backoff_factor *= 2;
    ++num_failures;
  } while (num_failures < 3);

  // Interrupt retry schedule with immediate request. On failure, it continues
  // the retry strategy using the next backoff.
  EXPECT_EQ(scheduler()->GetNumConsecutiveFailures(), num_failures);
  scheduler()->MakeImmediateRequest();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), kZeroTimeDuration);
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), num_failures + 1);
  FinishPendingRequest(/*success=*/false);
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(),
            std::min(kMaxRetryDuration,
                     kBaseRetryDuration * expected_backoff_factor));
  EXPECT_EQ(scheduler()->GetNumConsecutiveFailures(), num_failures + 1);
}

TEST_F(NearbyShareSchedulerBaseTest, StopScheduling_BeforeTimerFires) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();

  StartScheduling();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), kZeroTimeDuration);

  StopScheduling();
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());

  // Timer is still fired but owner is not notified.
  FastForward(kZeroTimeDuration);
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());

  // Scheduling restarts and pending task is rescheduled.
  StartScheduling();
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), 1u);
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbyShareSchedulerBaseTest, StopScheduling_BeforeResultIsHandled) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();

  StartScheduling();
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), 1u);

  StopScheduling();
  EXPECT_TRUE(scheduler()->IsWaitingForResult());

  // Although scheduling is stopped, the result can still be handled. No further
  // requests will be scheduled though.
  FinishPendingRequest(/*success=*/true);
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());
}

TEST_F(NearbyShareSchedulerBaseTest, RestoreRequest_InProgress) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();
  StartScheduling();
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), 1u);
  EXPECT_TRUE(scheduler()->IsWaitingForResult());
  DestroyScheduler();

  // On startup, set a pending immediate request because there was an
  // in-progress request at the time of shutdown.
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), kZeroTimeDuration);
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), 2u);
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbyShareSchedulerBaseTest, RestoreRequest_Pending_Immediate) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();
  StartScheduling();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), kZeroTimeDuration);
  DestroyScheduler();

  // On startup, set a pending immediate request because there was a pending
  // immediate request at the time of shutdown.
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), kZeroTimeDuration);
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), 1u);
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbyShareSchedulerBaseTest, RestoreRequest_Pending_FailureRetry) {
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();
  StartScheduling();

  // Fail three times then destroy scheduler.
  for (size_t num_failures = 0; num_failures < 3; ++num_failures) {
    ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
    EXPECT_EQ(on_request_call_count(), num_failures + 1);
    FinishPendingRequest(/*success=*/false);
  }
  absl::Duration initial_time_until_next_request =
      *scheduler()->GetTimeUntilNextRequest();
  EXPECT_EQ(initial_time_until_next_request, 4 * kBaseRetryDuration);
  DestroyScheduler();

  // 1s elapses while there is no scheduler. When the scheduler is recreated,
  // the retry request is rescheduled, accounting for the elapsed time.
  absl::Duration elapsed_time = absl::Seconds(1);
  FastForward(elapsed_time);
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  EXPECT_FALSE(scheduler()->IsWaitingForResult());
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), absl::Seconds(0));
  EXPECT_EQ(scheduler()->GetNumConsecutiveFailures(), 3u);
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), 4u);
  FinishPendingRequest(/*success=*/true);
}

TEST_F(NearbyShareSchedulerBaseTest, RestoreSchedulingData) {
  // Succeed immediately, then fail once before destroying the scheduler.
  absl::Time expected_last_success_time = Now() + absl::Seconds(100);
  FastForward(expected_last_success_time - Now());
  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  scheduler()->MakeImmediateRequest();
  StartScheduling();
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), 1u);
  FinishPendingRequest(/*success=*/true);
  scheduler()->MakeImmediateRequest();
  ASSERT_NO_FATAL_FAILURE(RunPendingRequest());
  EXPECT_EQ(on_request_call_count(), 2u);
  FinishPendingRequest(/*success=*/false);
  DestroyScheduler();

  CreateScheduler(/*retry_failures=*/true, /*require_connectivity=*/true);
  StartScheduling();
  EXPECT_EQ(scheduler()->GetLastSuccessTime(), expected_last_success_time);
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), absl::Seconds(0));
  EXPECT_EQ(scheduler()->GetNumConsecutiveFailures(), 1u);
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
