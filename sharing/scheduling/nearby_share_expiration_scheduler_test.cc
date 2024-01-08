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

#include "sharing/scheduling/nearby_share_expiration_scheduler.h"

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
constexpr absl::Duration kTestInitialNow = absl::Hours(2400);
constexpr absl::Duration kTestExpirationTimeFromInitialNow = absl::Minutes(123);

class NearbyShareExpirationSchedulerTest : public ::testing::Test {
 protected:
  NearbyShareExpirationSchedulerTest() = default;
  ~NearbyShareExpirationSchedulerTest() override = default;

  void SetUp() override {
    FastForward(kTestInitialNow);
    expiration_time_ = Now() + kTestExpirationTimeFromInitialNow;

    preference_manager_.Remove(kTestPrefName);

    scheduler_ = std::make_unique<NearbyShareExpirationScheduler>(
        &fake_context_, preference_manager_, callback_,
        /*retry_failures=*/true, /*require_connectivity=*/true, kTestPrefName,
        nullptr);
  }

  absl::Time Now() const { return fake_context_.GetClock()->Now(); }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(absl::Duration delta) {
    fake_context_.fake_clock()->FastForward(delta);
  }

  std::optional<absl::Time> expiration_time_;
  NearbyShareScheduler* scheduler() { return scheduler_.get(); }

 private:
  nearby::FakePreferenceManager preference_manager_;
  nearby::FakeContext fake_context_;
  std::unique_ptr<NearbyShareScheduler> scheduler_ = nullptr;
  NearbyShareExpirationScheduler::ExpirationTimeFunctor callback_ = [&]() {
    return expiration_time_;
  };
};

TEST_F(NearbyShareExpirationSchedulerTest, ExpirationRequest) {
  scheduler()->Start();

  // Wait 5 minutes to make sure the time to the next request only depends on
  // the expiration time and the current time.
  FastForward(absl::Minutes(5));

  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), *expiration_time_ - Now());
}

TEST_F(NearbyShareExpirationSchedulerTest, Reschedule) {
  scheduler()->Start();
  FastForward(absl::Minutes(5));

  absl::Duration initial_expected_time_until_next_request =
      *expiration_time_ - Now();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(),
            initial_expected_time_until_next_request);

  // The expiration time suddenly changes.
  expiration_time_ = *expiration_time_ + absl::Hours(48);
  scheduler()->Reschedule();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(),
            initial_expected_time_until_next_request + absl::Hours(48));
}

TEST_F(NearbyShareExpirationSchedulerTest, NullExpirationTime) {
  expiration_time_.reset();
  scheduler()->Start();
  EXPECT_EQ(scheduler()->GetTimeUntilNextRequest(), std::nullopt);
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
