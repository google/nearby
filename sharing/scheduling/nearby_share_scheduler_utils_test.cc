// Copyright 2023 Google LLC
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

#include "sharing/scheduling/nearby_share_scheduler_utils.h"

#include <cstdint>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/scheduling/nearby_share_scheduler_fields.h"

namespace nearby::sharing {

using ::nearby::sharing::api::PreferenceManager;
using ::testing::Eq;

class NearbyShareSchedulerUtilsTest : public ::testing::Test {
 protected:
  NearbyShareSchedulerUtilsTest() = default;
  ~NearbyShareSchedulerUtilsTest() override = default;

  void CreateSchedule(absl::string_view pref_name,
                      std::optional<int64_t> attempt_time,
                      std::optional<int64_t> success_time,
                      std::optional<int64_t> failed_count,
                      std::optional<bool> has_pending,
                      std::optional<bool> is_waiting) {
    if (attempt_time.has_value()) {
      preference_manager_.SetDictionaryInt64Value(
          pref_name, SchedulerFields::kLastAttemptTimeKeyName,
          attempt_time.value());
    }
    if (success_time.has_value()) {
      preference_manager_.SetDictionaryInt64Value(
          pref_name, SchedulerFields::kLastSuccessTimeKeyName,
          success_time.value());
    }
    if (failed_count.has_value()) {
      preference_manager_.SetDictionaryInt64Value(
          pref_name, SchedulerFields::kNumConsecutiveFailuresKeyName,
          failed_count.value());
    }
    if (has_pending.has_value()) {
      preference_manager_.SetDictionaryBooleanValue(
          pref_name, SchedulerFields::kHasPendingImmediateRequestKeyName,
          has_pending.value());
    }
    if (is_waiting.has_value()) {
      preference_manager_.SetDictionaryBooleanValue(
          pref_name, SchedulerFields::kIsWaitingForResultKeyName,
          is_waiting.value());
    }
  }

  PreferenceManager& preference_manager() { return preference_manager_; }

 private:
  nearby::FakePreferenceManager preference_manager_;
};

TEST_F(NearbyShareSchedulerUtilsTest, ConvertToReadableScheduleSucceeds) {
  absl::string_view test_pref = "test_pref";
  preference_manager().Remove(test_pref);
  CreateSchedule(test_pref, /*attempt_time=*/123456L, /*success_time=*/345678L,
                 /*failed_count=*/123, /*has_pending=*/true,
                 /*is_waiting=*/false);

  std::string debug_str =
      ConvertToReadableSchedule(preference_manager(), test_pref);

  EXPECT_THAT(debug_str, Eq("{attempt_time:1969-12-31 16:00:00, "
                            "success_time:1969-12-31 16:00:00, "
                            "failed_count:123, has_pending_request:true, "
                            "is_waiting_for_result:false}"));
}

TEST_F(NearbyShareSchedulerUtilsTest, ConvertToReadableScheduleEmpty) {
  absl::string_view test_pref = "test_pref";
  preference_manager().Remove(test_pref);

  std::string debug_str =
      ConvertToReadableSchedule(preference_manager(), test_pref);

  EXPECT_THAT(debug_str, Eq("{}"));
}

TEST_F(NearbyShareSchedulerUtilsTest, ConvertToReadableScheduleMissingFields) {
  absl::string_view test_pref = "test_pref";
  preference_manager().Remove(test_pref);
  CreateSchedule(test_pref, /*attempt_time=*/std::nullopt,
                 /*success_time=*/std::nullopt,
                 /*failed_count=*/345, /*has_pending=*/std::nullopt,
                 /*is_waiting=*/true);

  std::string debug_str =
      ConvertToReadableSchedule(preference_manager(), test_pref);

  EXPECT_THAT(debug_str, Eq("{failed_count:345, is_waiting_for_result:true}"));
}

}  // namespace nearby::sharing
