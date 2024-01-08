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

#include "sharing/scheduling/nearby_share_on_demand_scheduler.h"

#include <memory>

#include "gtest/gtest.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/scheduling/nearby_share_scheduler.h"

namespace nearby {
namespace sharing {
namespace {

const char kTestPrefName[] = "test_pref_name";

class NearbyShareOnDemandSchedulerTest : public ::testing::Test {
 protected:
  NearbyShareOnDemandSchedulerTest() = default;
  ~NearbyShareOnDemandSchedulerTest() override = default;

  void SetUp() override {
    preference_manager_.Remove(kTestPrefName);

    scheduler_ = std::make_unique<NearbyShareOnDemandScheduler>(
        &fake_context_, preference_manager_,
        /*retry_failures=*/true, /*require_connectivity=*/true, kTestPrefName,
        nullptr);
  }

  NearbyShareScheduler* scheduler() { return scheduler_.get(); }

 private:
  nearby::FakePreferenceManager preference_manager_;
  FakeContext fake_context_;
  std::unique_ptr<NearbyShareScheduler> scheduler_;
};

TEST_F(NearbyShareOnDemandSchedulerTest, NoRecurringRequest) {
  scheduler()->Start();
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
