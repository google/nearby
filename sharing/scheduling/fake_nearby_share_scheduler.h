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

#ifndef THIRD_PARTY_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_H_
#define THIRD_PARTY_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_H_

#include <stddef.h>

#include <optional>
#include <vector>

#include "absl/time/time.h"
#include "sharing/scheduling/nearby_share_scheduler.h"

namespace nearby {
namespace sharing {

// A fake implementation of NearbyShareScheduler that allows the user to set all
// scheduling data. It tracks the number of immediate requests and the handled
// results. The on-request callback can be invoked using
// InvokeRequestCallback().
class FakeNearbyShareScheduler : public NearbyShareScheduler {
 public:
  explicit FakeNearbyShareScheduler(OnRequestCallback callback);
  ~FakeNearbyShareScheduler() override;

  // NearbyShareScheduler:
  void MakeImmediateRequest() override;
  void HandleResult(bool success) override;
  void Reschedule() override;
  std::optional<absl::Time> GetLastSuccessTime() const override;
  std::optional<absl::Duration> GetTimeUntilNextRequest() const override;
  bool IsWaitingForResult() const override;
  size_t GetNumConsecutiveFailures() const override;

  void SetLastSuccessTime(std::optional<absl::Time> time);
  void SetTimeUntilNextRequest(std::optional<absl::Duration> time_delta);
  void SetIsWaitingForResult(bool is_waiting);
  void SetNumConsecutiveFailures(size_t num_failures);

  void InvokeRequestCallback();

  size_t num_immediate_requests() const { return num_immediate_requests_; }
  size_t num_reschedule_calls() const { return num_reschedule_calls_; }
  const std::vector<bool>& handled_results() const { return handled_results_; }

 private:
  // NearbyShareScheduler:
  void OnStart() override;
  void OnStop() override;

  bool can_invoke_request_callback_ = false;
  size_t num_immediate_requests_ = 0;
  size_t num_reschedule_calls_ = 0;
  std::vector<bool> handled_results_;
  std::optional<absl::Time> last_success_time_;
  std::optional<absl::Duration> time_until_next_request_;
  bool is_waiting_for_result_ = false;
  size_t num_consecutive_failures_ = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_H_
