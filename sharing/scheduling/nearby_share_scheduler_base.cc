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
#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "internal/platform/single_thread_executor.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/scheduling/format.h"
#include "sharing/scheduling/nearby_share_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler_fields.h"
#include "sharing/thread_timer.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::api::PreferenceManager;

constexpr absl::Duration kBaseRetryDelay = absl::Seconds(5);
constexpr absl::Duration kMaxRetryDelay = absl::Hours(1);

}  // namespace

NearbyShareSchedulerBase::NearbyShareSchedulerBase(
    Context* context, PreferenceManager& preference_manager,
    SingleThreadExecutor* absl_nonnull executor, bool retry_failures,
    bool require_connectivity, absl::string_view pref_name,
    OnRequestCallback callback)
    : NearbyShareScheduler(std::move(callback)),
      connectivity_manager_(context->GetConnectivityManager()),
      preference_manager_(preference_manager),
      executor_(executor),
      clock_(context->GetClock()),
      retry_failures_(retry_failures),
      require_connectivity_(require_connectivity),
      pref_name_(pref_name),
      connection_listener_name_(absl::Substitute("scheduler-$0", pref_name_)) {
  DCHECK(executor_ != nullptr);

  // On startup, set a pending immediate request if the pref service indicates
  // that there was an in-progress request or a pending immediate request at the
  // time of shutdown.
  bool is_waiting = IsWaitingForResult();
  if (!is_waiting) {
    // If scheduler has failed, speed up the data sync when there are issues.
    if (GetNumConsecutiveFailures() > 0) {
      LOG(WARNING) << ": Run the scheduler " << pref_name_
                   << " immediately due to having failed runs.";
      is_waiting = true;
    }
  }
  if (is_waiting) {
    SetHasPendingImmediateRequest(true);
    SetIsWaitingForResult(false);
  }

  if (require_connectivity_) {
    connectivity_manager_->RegisterInternetListener(
        connection_listener_name_,
        [this, is_destroyed = is_destroyed_](bool is_internet_connected) {
          if (!is_internet_connected) {
            return;
          }
          executor_->Execute([this, is_destroyed]() {
            if (is_destroyed->load(std::memory_order_seq_cst)) {
              return;
            }
            OnInternetConnectivityChanged(true);
          });
        });
  }
}

NearbyShareSchedulerBase::~NearbyShareSchedulerBase() {
  if (require_connectivity_) {
    connectivity_manager_->UnregisterInternetListener(
        connection_listener_name_);
  }
  is_destroyed_->store(true, std::memory_order_seq_cst);
  timer_.reset();
}

void NearbyShareSchedulerBase::MakeImmediateRequest() {
  timer_.reset();
  SetHasPendingImmediateRequest(true);
  Reschedule();
}

void NearbyShareSchedulerBase::HandleResult(bool success) {
  absl::Time now = clock_->Now();
  SetLastAttemptTime(now);

  LOG(INFO) << "Nearby Share scheduler \"" << pref_name_ << "\" latest attempt "
            << (success ? "succeeded" : "failed");

  if (success) {
    SetLastSuccessTime(now);
    SetNumConsecutiveFailures(0);
  } else {
    SetNumConsecutiveFailures(GetNumConsecutiveFailures() + 1);
  }

  SetIsWaitingForResult(false);
  Reschedule();
}

void NearbyShareSchedulerBase::Reschedule() {
  if (!is_running()) return;

  timer_.reset();

  absl::Duration delay = GetTimeUntilNextRequest();
  if (delay == absl::InfiniteDuration()) {
    LOG(INFO) << "Task \"" << pref_name_ << "\"" << " not scheduled";
  } else {
    LOG(INFO) << "Task \"" << pref_name_ << "\"" << " scheduled in " << delay;
    timer_ = std::make_unique<ThreadTimer>(
        *executor_, absl::StrCat("scheduler_", pref_name_), delay,
        [this]() { OnTimerFired(); });
  }
  PrintSchedulerState(delay);
}

absl::Time NearbyShareSchedulerBase::GetLastSuccessTime() const {
  std::optional<int64_t> pref_value =
      preference_manager_.GetDictionaryInt64Value(
          pref_name_, SchedulerFields::kLastSuccessTimeKeyName);
  if (!pref_value.has_value()) {
    return absl::InfinitePast();
  }
  return absl::FromUnixNanos(pref_value.value());
}

absl::Duration NearbyShareSchedulerBase::GetTimeUntilNextRequest() const {
  if (!is_running() || IsWaitingForResult()) return absl::InfiniteDuration();

  if (HasPendingImmediateRequest()) return absl::ZeroDuration();

  absl::Time now = clock_->Now();

  // Recover from failures using exponential backoff strategy if necessary.
  absl::Duration time_until_retry = TimeUntilRetry(now);
  if (time_until_retry != absl::InfiniteDuration()) return time_until_retry;

  // Schedule the periodic request if applicable.
  return TimeUntilRecurringRequest(now);
}

bool NearbyShareSchedulerBase::IsWaitingForResult() const {
  std::optional<bool> pref_value =
      preference_manager_.GetDictionaryBooleanValue(
          pref_name_, SchedulerFields::kIsWaitingForResultKeyName);

  bool is_waiting = false;
  if (pref_value.has_value()) {
    is_waiting = pref_value.value();
  }
  return is_waiting;
}

size_t NearbyShareSchedulerBase::GetNumConsecutiveFailures() const {
  std::optional<int64_t> pref_value =
      preference_manager_.GetDictionaryInt64Value(
          pref_name_, SchedulerFields::kNumConsecutiveFailuresKeyName);

  if (!pref_value.has_value()) {
    return 0;
  }
  return pref_value.value();
}

void NearbyShareSchedulerBase::OnStart() { Reschedule(); }

void NearbyShareSchedulerBase::OnStop() { timer_.reset(); }

void NearbyShareSchedulerBase::OnInternetConnectivityChanged(
    bool is_internet_connected) {
  if (!is_internet_connected) {
    return;
  }
  LOG(INFO) << "Internet connectivity restored for scheduler: " << pref_name_;
  Reschedule();
}

absl::Time NearbyShareSchedulerBase::GetLastAttemptTime() const {
  std::optional<int64_t> pref_value =
      preference_manager_.GetDictionaryInt64Value(
          pref_name_, SchedulerFields::kLastAttemptTimeKeyName);
  if (!pref_value.has_value()) {
    return absl::InfinitePast();
  }
  return absl::FromUnixNanos(pref_value.value());
}

bool NearbyShareSchedulerBase::HasPendingImmediateRequest() const {
  std::optional<bool> pref_value =
      preference_manager_.GetDictionaryBooleanValue(
          pref_name_, SchedulerFields::kHasPendingImmediateRequestKeyName);
  if (!pref_value.has_value()) {
    return false;
  }
  return pref_value.value();
}

void NearbyShareSchedulerBase::SetLastAttemptTime(
    absl::Time last_attempt_time) {
  preference_manager_.SetDictionaryInt64Value(
      pref_name_, SchedulerFields::kLastAttemptTimeKeyName,
      absl::ToUnixNanos(last_attempt_time));
}

void NearbyShareSchedulerBase::SetLastSuccessTime(
    absl::Time last_success_time) {
  preference_manager_.SetDictionaryInt64Value(
      pref_name_, SchedulerFields::kLastSuccessTimeKeyName,
      absl::ToUnixNanos(last_success_time));
}

void NearbyShareSchedulerBase::SetNumConsecutiveFailures(size_t num_failures) {
  preference_manager_.SetDictionaryInt64Value(
      pref_name_, SchedulerFields::kNumConsecutiveFailuresKeyName,
      num_failures);
}

void NearbyShareSchedulerBase::SetHasPendingImmediateRequest(
    bool has_pending_immediate_request) {
  preference_manager_.SetDictionaryBooleanValue(
      pref_name_, SchedulerFields::kHasPendingImmediateRequestKeyName,
      has_pending_immediate_request);
}

void NearbyShareSchedulerBase::SetIsWaitingForResult(
    bool is_waiting_for_result) {
  preference_manager_.SetDictionaryBooleanValue(
      pref_name_, SchedulerFields::kIsWaitingForResultKeyName,
      is_waiting_for_result);
}

absl::Duration NearbyShareSchedulerBase::TimeUntilRetry(absl::Time now) const {
  if (!retry_failures_) return absl::InfiniteDuration();

  size_t num_failures = GetNumConsecutiveFailures();
  if (num_failures == 0) return absl::InfiniteDuration();

  // The exponential back off is
  //
  //   base * 2^(num_failures - 1)
  //
  // up to a fixed maximum retry delay.
  absl::Duration delay =
      std::min(kMaxRetryDelay, kBaseRetryDelay * (1 << (num_failures - 1)));

  absl::Duration time_elapsed_since_last_attempt = now - GetLastAttemptTime();

  return std::max(absl::ZeroDuration(),
                  delay - time_elapsed_since_last_attempt);
}

void NearbyShareSchedulerBase::OnTimerFired() {
  timer_.reset();
  if (!is_running()) {
    LOG(DFATAL) << "Timer fired after stop for scheduler: " << pref_name_;
    return;
  }
  if (require_connectivity_ && !connectivity_manager_->IsInternetConnected()) {
    LOG(INFO) << "Task \"" << pref_name_
              << "\" ignored, no internet connection";
    return;
  }

  SetIsWaitingForResult(true);
  SetHasPendingImmediateRequest(false);
  NotifyOfRequest();
}

void NearbyShareSchedulerBase::PrintSchedulerState(
    absl::Duration time_until_next_request) const {
  if (!VLOG_IS_ON(1)) {
    return;
  }
  absl::Time last_attempt_time = GetLastAttemptTime();
  absl::Time last_success_time = GetLastSuccessTime();

  std::stringstream ss;
  ss << "State of Nearby Share scheduler \"" << pref_name_ << "\":"
     << "\n  Last attempt time: ";
  if (last_attempt_time != absl::InfinitePast()) {
    ss << nearby::utils::TimeFormatShortDateAndTimeWithTimeZone(
        last_attempt_time);
  } else {
    ss << "Never";
  }

  ss << "\n  Last success time: ";
  if (last_success_time != absl::InfinitePast()) {
    ss << nearby::utils::TimeFormatShortDateAndTimeWithTimeZone(
        last_success_time);
  } else {
    ss << "Never";
  }

  ss << "\n  Time until next request: ";
  if (time_until_next_request != absl::InfiniteDuration()) {
    ss << time_until_next_request;
  } else {
    ss << "Never";
  }

  ss << "\n  Is waiting for result? " << (IsWaitingForResult() ? "Yes" : "No");
  ss << "\n  Pending immediate request? "
     << (HasPendingImmediateRequest() ? "Yes" : "No");
  ss << "\n  Num consecutive failures: " << GetNumConsecutiveFailures();

  VLOG(1) << ss.str();
}

}  // namespace sharing
}  // namespace nearby
