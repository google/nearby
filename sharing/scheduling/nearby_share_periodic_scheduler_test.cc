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

#include "sharing/scheduling/nearby_share_periodic_scheduler.h"

#include <memory>
#include <optional>

#include "gtest/gtest.h"
#include "absl/time/time.h"
#include "internal/test/fake_clock.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/scheduling/nearby_share_scheduler.h"

namespace nearby {
namespace sharing {
namespace {

const char kTestPrefName[] = "test_pref_name";
constexpr absl::Duration kTestRequestPeriod = absl::Minutes(123);

class NearbySharePeriodicSchedulerTest : public ::testing::Test {
 protected:
  NearbySharePeriodicSchedulerTest() = default;
  ~NearbySharePeriodicSchedulerTest() override = default;

  void SetUp() override {
    preference_manager_.Remove(kTestPrefName);

    scheduler_ = std::make_unique<NearbySharePeriodicScheduler>(
        &fake_context_, preference_manager_, kTestRequestPeriod,
        /*retry_failures=*/true,
        /*require_connectivity=*/true, kTestPrefName, nullptr);
  }

  absl::Time Now() const { return fake_context_.GetClock()->Now(); }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(absl::Duration delta) {
    fake_context_.fake_clock()->FastForward(delta);
  }

  NearbyShareScheduler* scheduler() { return scheduler_.get(); }

 private:
  nearby::FakePreferenceManager preference_manager_;
  FakeContext fake_context_;
  std::unique_ptr<NearbyShareScheduler> scheduler_;
};

TEST_F(NearbySharePeriodicSchedulerTest, PeriodicRequest) {
  // Set Now() to something nontrivial.
  FastForward(absl::Hours(2400));

  // Immediately runs a first-time periodic request.
  scheduler()->Start();
  std::optional<absl::Duration> time_until_next_request =
      scheduler()->GetTimeUntilNextRequest();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), absl::ZeroDuration());
  FastForward(*time_until_next_request);
  scheduler()->HandleResult(/*success=*/true);
  EXPECT_EQ(scheduler()->GetLastSuccessTime(), Now());

  // Wait 1 minute since the last success.
  absl::Duration elapsed_time = absl::Minutes(1);
  FastForward(elapsed_time);

  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(),
            kTestRequestPeriod - elapsed_time);
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
