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

#include "sharing/scheduling/fake_nearby_share_scheduler.h"

#include <stddef.h>

#include <optional>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "sharing/internal/public/logging.h"
#include "sharing/scheduling/nearby_share_scheduler.h"

namespace nearby {
namespace sharing {

FakeNearbyShareScheduler::FakeNearbyShareScheduler(OnRequestCallback callback)
    : NearbyShareScheduler(std::move(callback)) {}

FakeNearbyShareScheduler::~FakeNearbyShareScheduler() = default;

void FakeNearbyShareScheduler::MakeImmediateRequest() {
  ++num_immediate_requests_;
}

void FakeNearbyShareScheduler::HandleResult(bool success) {
  handled_results_.push_back(success);
}

void FakeNearbyShareScheduler::Reschedule() { ++num_reschedule_calls_; }

std::optional<absl::Time> FakeNearbyShareScheduler::GetLastSuccessTime() const {
  return last_success_time_;
}

std::optional<absl::Duration>
FakeNearbyShareScheduler::GetTimeUntilNextRequest() const {
  return time_until_next_request_;
}

bool FakeNearbyShareScheduler::IsWaitingForResult() const {
  return is_waiting_for_result_;
}

size_t FakeNearbyShareScheduler::GetNumConsecutiveFailures() const {
  return num_consecutive_failures_;
}

void FakeNearbyShareScheduler::OnStart() {
  can_invoke_request_callback_ = true;
}

void FakeNearbyShareScheduler::OnStop() {
  can_invoke_request_callback_ = false;
}

void FakeNearbyShareScheduler::InvokeRequestCallback() {
  NL_DCHECK(can_invoke_request_callback_);
  NotifyOfRequest();
}

void FakeNearbyShareScheduler::SetLastSuccessTime(
    std::optional<absl::Time> time) {
  last_success_time_ = time;
}

void FakeNearbyShareScheduler::SetTimeUntilNextRequest(
    std::optional<absl::Duration> time_delta) {
  time_until_next_request_ = time_delta;
}

void FakeNearbyShareScheduler::SetIsWaitingForResult(bool is_waiting) {
  is_waiting_for_result_ = is_waiting;
}

void FakeNearbyShareScheduler::SetNumConsecutiveFailures(size_t num_failures) {
  num_consecutive_failures_ = num_failures;
}

}  // namespace sharing
}  // namespace nearby
