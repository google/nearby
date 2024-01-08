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
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/scheduling/nearby_share_scheduler_fields.h"

namespace nearby::sharing {

using ::nearby::sharing::api::PreferenceManager;

std::string ConvertToReadableSchedule(PreferenceManager& preference_manager,
                                      absl::string_view schedule_preference) {
  std::string result = "{";
  std::optional<int64_t> attempt_time =
      preference_manager.GetDictionaryInt64Value(
          schedule_preference, SchedulerFields::kLastAttemptTimeKeyName);
  if (attempt_time.has_value()) {
    std::time_t local_t =
        absl::ToTimeT(absl::FromUnixNanos(attempt_time.value()));
    std::tm* local_time = std::localtime(&local_t);
    std::stringstream buffer;
    buffer << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");
    absl::StrAppendFormat(&result, "attempt_time:%s, ", buffer.str());
  }
  std::optional<int64_t> success_time =
      preference_manager.GetDictionaryInt64Value(
          schedule_preference, SchedulerFields::kLastSuccessTimeKeyName);
  if (success_time.has_value()) {
    std::time_t local_t =
        absl::ToTimeT(absl::FromUnixNanos(success_time.value()));
    std::tm* local_time = std::localtime(&local_t);
    std::stringstream buffer;
    buffer << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");
    absl::StrAppendFormat(&result, "success_time:%s, ", buffer.str());
  }
  std::optional<int64_t> failed_count =
      preference_manager.GetDictionaryInt64Value(
          schedule_preference, SchedulerFields::kNumConsecutiveFailuresKeyName);
  if (failed_count.has_value()) {
    absl::StrAppendFormat(&result, "failed_count:%d, ", failed_count.value());
  }
  std::optional<bool> has_pending =
      preference_manager.GetDictionaryBooleanValue(
          schedule_preference,
          SchedulerFields::kHasPendingImmediateRequestKeyName);
  if (has_pending.has_value()) {
    absl::StrAppendFormat(&result, "has_pending_request:%s, ",
                          has_pending.value() ? "true" : "false");
  }
  std::optional<bool> is_waiting = preference_manager.GetDictionaryBooleanValue(
      schedule_preference, SchedulerFields::kIsWaitingForResultKeyName);
  if (is_waiting.has_value()) {
    absl::StrAppendFormat(&result, "is_waiting_for_result:%s",
                          is_waiting.value() ? "true" : "false");
  }

  absl::StrAppend(&result, "}");
  return result;
}

}  // namespace nearby::sharing
