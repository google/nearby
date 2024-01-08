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
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/internal/public/logging.h"
#include "sharing/scheduling/format.h"
#include "sharing/scheduling/nearby_share_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler_fields.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::api::PreferenceManager;

constexpr absl::Duration kZeroTimeDelta = absl::ZeroDuration();
constexpr absl::Duration kBaseRetryDelay = absl::Seconds(5);
constexpr absl::Duration kMaxRetryDelay = absl::Hours(1);

}  // namespace

NearbyShareSchedulerBase::NearbyShareSchedulerBase(
    Context* context, PreferenceManager& preference_manager,
    bool retry_failures, bool require_connectivity, absl::string_view pref_name,
    OnRequestCallback callback)
    : NearbyShareScheduler(std::move(callback)),
      connectivity_manager_(context->GetConnectivityManager()),
      preference_manager_(preference_manager),
      clock_(context->GetClock()),
      retry_failures_(retry_failures),
      require_connectivity_(require_connectivity),
      pref_name_(pref_name) {
  timer_ = context->CreateTimer();
  connection_listener_name_ = absl::Substitute(
      "scheduler-$0-$1", pref_name_, absl::ToUnixNanos(absl::UnixEpoch()));

  InitializePersistedRequest();
  is_initialized_ = true;

  if (require_connectivity_) {
    connectivity_manager_->RegisterConnectionListener(
        connection_listener_name_,
        [this](nearby::ConnectivityManager::ConnectionType connection_type,
               bool is_lan_connected) {
          OnConnectionChanged(connection_type);
        });
  }
}

NearbyShareSchedulerBase::~NearbyShareSchedulerBase() {
  if (require_connectivity_) {
    connectivity_manager_->UnregisterConnectionListener(
        connection_listener_name_);
  }
}

void NearbyShareSchedulerBase::MakeImmediateRequest() {
  timer_->Stop();
  SetHasPendingImmediateRequest(true);
  Reschedule();
}

void NearbyShareSchedulerBase::HandleResult(bool success) {
  absl::Time now = clock_->Now();
  SetLastAttemptTime(now);

  NL_LOG(INFO) << "Nearby Share scheduler \"" << pref_name_
               << "\" latest attempt " << (success ? "succeeded" : "failed");

  if (success) {
    SetLastSuccessTime(now);
    SetNumConsecutiveFailures(0);
  } else {
    SetNumConsecutiveFailures(GetNumConsecutiveFailures() + 1);
  }

  SetIsWaitingForResult(false);
  Reschedule();
  PrintSchedulerState();
}

void NearbyShareSchedulerBase::Reschedule() {
  if (!is_running()) return;

  timer_->Stop();

  std::optional<absl::Duration> delay = GetTimeUntilNextRequest();
  if (!delay.has_value()) return;

  int64_t delay_milliseconds = (*delay) / absl::Milliseconds(1);

  timer_->Start(delay_milliseconds, delay_milliseconds,
                [this]() { OnTimerFired(); });
}

std::optional<absl::Time> NearbyShareSchedulerBase::GetLastSuccessTime() const {
  std::optional<int64_t> pref_value =
      preference_manager_.GetDictionaryInt64Value(
          pref_name_, SchedulerFields::kLastSuccessTimeKeyName);
  if (!pref_value.has_value()) {
    return std::nullopt;
  }
  return absl::FromUnixNanos(pref_value.value());
}

std::optional<absl::Duration>
NearbyShareSchedulerBase::GetTimeUntilNextRequest() const {
  if (!is_running() || IsWaitingForResult()) return std::nullopt;

  if (HasPendingImmediateRequest()) return kZeroTimeDelta;

  absl::Time now = clock_->Now();

  // Recover from failures using exponential backoff strategy if necessary.
  std::optional<absl::Duration> time_until_retry = TimeUntilRetry(now);
  if (time_until_retry) return time_until_retry;

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

  if (is_waiting) {
    return true;
  }

  // The scheduler must be initialized if it is not initialized or has failed.
  // This will speed up the data sync when there are issues.
  if (!is_initialized_) {
    if (GetNumConsecutiveFailures() > 0) {
      NL_LOG(WARNING) << ": Run the scheduler " << pref_name_
                      << " immediately due to having failed runs.";
      return true;
    }
  }

  return false;
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

void NearbyShareSchedulerBase::OnStart() {
  Reschedule();
  NL_LOG(INFO) << "Starting Nearby Share scheduler \"" << pref_name_ << "\"";
  PrintSchedulerState();
}

void NearbyShareSchedulerBase::OnStop() { timer_->Stop(); }

void NearbyShareSchedulerBase::OnConnectionChanged(
    nearby::ConnectivityManager::ConnectionType connection_type) {
  if (connection_type == nearby::ConnectivityManager::ConnectionType::kNone)
    return;

  Reschedule();
}

std::optional<absl::Time> NearbyShareSchedulerBase::GetLastAttemptTime() const {
  std::optional<int64_t> pref_value =
      preference_manager_.GetDictionaryInt64Value(
          pref_name_, SchedulerFields::kLastAttemptTimeKeyName);
  if (!pref_value.has_value()) {
    return std::nullopt;
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

void NearbyShareSchedulerBase::InitializePersistedRequest() {
  if (IsWaitingForResult()) {
    SetHasPendingImmediateRequest(true);
    SetIsWaitingForResult(false);
  }
}

std::optional<absl::Duration> NearbyShareSchedulerBase::TimeUntilRetry(
    absl::Time now) const {
  if (!retry_failures_) return std::nullopt;

  size_t num_failures = GetNumConsecutiveFailures();
  if (num_failures == 0) return std::nullopt;

  // The exponential back off is
  //
  //   base * 2^(num_failures - 1)
  //
  // up to a fixed maximum retry delay.
  absl::Duration delay =
      std::min(kMaxRetryDelay, kBaseRetryDelay * (1 << (num_failures - 1)));

  absl::Duration time_elapsed_since_last_attempt = now - *GetLastAttemptTime();

  return std::max(kZeroTimeDelta, delay - time_elapsed_since_last_attempt);
}

void NearbyShareSchedulerBase::OnTimerFired() {
  NL_DCHECK(is_running());
  if (require_connectivity_ &&
      (connectivity_manager_->GetConnectionType() ==
       nearby::ConnectivityManager::ConnectionType::kNone)) {
    return;
  }

  SetIsWaitingForResult(true);
  SetHasPendingImmediateRequest(false);
  NotifyOfRequest();
}

void NearbyShareSchedulerBase::PrintSchedulerState() const {
  std::optional<absl::Time> last_attempt_time = GetLastAttemptTime();
  std::optional<absl::Time> last_success_time = GetLastSuccessTime();
  std::optional<absl::Duration> time_until_next_request =
      GetTimeUntilNextRequest();

  std::stringstream ss;
  ss << "State of Nearby Share scheduler \"" << pref_name_ << "\":"
     << "\n  Last attempt time: ";
  if (last_attempt_time) {
    ss << nearby::utils::TimeFormatShortDateAndTimeWithTimeZone(
        *last_attempt_time);
  } else {
    ss << "Never";
  }

  ss << "\n  Last success time: ";
  if (last_success_time) {
    ss << nearby::utils::TimeFormatShortDateAndTimeWithTimeZone(
        *last_success_time);
  } else {
    ss << "Never";
  }

  ss << "\n  Time until next request: ";
  if (time_until_next_request) {
    std::u16string next_request_delay;
    ss << nearby::utils::TimeDurationFormatWithSeconds(
        *time_until_next_request);
  } else {
    ss << "Never";
  }

  ss << "\n  Is waiting for result? " << (IsWaitingForResult() ? "Yes" : "No");
  ss << "\n  Pending immediate request? "
     << (HasPendingImmediateRequest() ? "Yes" : "No");
  ss << "\n  Num consecutive failures: " << GetNumConsecutiveFailures();

  NL_VLOG(1) << ss.str();
}

}  // namespace sharing
}  // namespace nearby
