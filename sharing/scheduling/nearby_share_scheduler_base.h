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

#ifndef THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_BASE_H_
#define THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_BASE_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/scheduling/nearby_share_scheduler.h"

namespace nearby {
namespace sharing {

// A base NearbyShareScheduler implementation that persists scheduling data.
// Requests made before scheduling has started, while another attempt is in
// progress, or while offline are cached and rescheduled as soon as possible.
// Likewise, when the scheduler is stopped or destroyed, scheduling data is
// persisted and restored when the scheduler is restarted or recreated,
// respectively.
//
// If automatic failure retry is enabled, all failed attempts follow an
// exponential backoff retry strategy.
//
// The scheduler waits until the device is online before notifying the owner if
// network connectivity is required.
//
// Derived classes must override TimeUntilRecurringRequest() to establish the
// desired recurring request behavior of the scheduler.
class NearbyShareSchedulerBase : public NearbyShareScheduler {
 public:
  ~NearbyShareSchedulerBase() override;

 protected:
  // |context|: Nearby context, holding nearby common components.
  // |retry_failures|: Whether or not automatically retry failures using
  //     exponential backoff strategy.
  // |require_connectivity|: If true, the scheduler will not alert the owner of
  //     a request until network connectivity is established.
  // |pref_name|: The dictionary pref name used to persist scheduling data. Make
  //     sure to register this pref name before creating the scheduler.
  // |callback|: The function invoked to alert the owner that a request is due.
  NearbyShareSchedulerBase(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      bool retry_failures, bool require_connectivity,
      absl::string_view pref_name, OnRequestCallback callback);

  // The time to wait until the next regularly recurring request.
  virtual std::optional<absl::Duration> TimeUntilRecurringRequest(
      absl::Time now) const = 0;

  // NearbyShareScheduler:
  void MakeImmediateRequest() override;
  void HandleResult(bool success) override;
  void Reschedule() override;
  std::optional<absl::Time> GetLastSuccessTime() const override;
  std::optional<absl::Duration> GetTimeUntilNextRequest() const override;
  bool IsWaitingForResult() const override;
  size_t GetNumConsecutiveFailures() const override;
  void OnStart() override;
  void OnStop() override;

  void OnConnectionChanged(
      nearby::ConnectivityManager::ConnectionType connection_type);

  std::optional<absl::Time> GetLastAttemptTime() const;
  bool HasPendingImmediateRequest() const;

  // Set and persist scheduling data in prefs.
  void SetLastAttemptTime(absl::Time last_attempt_time);
  void SetLastSuccessTime(absl::Time last_success_time);
  void SetNumConsecutiveFailures(size_t num_failures);
  void SetHasPendingImmediateRequest(bool has_pending_immediate_request);
  void SetIsWaitingForResult(bool is_waiting_for_result);

  // On startup, set a pending immediate request if the pref service indicates
  // that there was an in-progress request or a pending immediate request at the
  // time of shutdown.
  void InitializePersistedRequest();

  // The amount of time to wait until the next automatic failure retry. Returns
  // std::nullopt if there is no failure to retry or if failure retry is not
  // enabled for the scheduler.
  std::optional<absl::Duration> TimeUntilRetry(absl::Time now) const;

  // Notifies the owner that a request is ready. Early returns if not online and
  // the scheduler requires connectivity; the attempt is rescheduled when
  // connectivity is restored.
  void OnTimerFired();

  void PrintSchedulerState() const;

 private:
  nearby::ConnectivityManager* const connectivity_manager_;
  nearby::sharing::api::PreferenceManager& preference_manager_;
  const nearby::Clock* const clock_;

  const bool retry_failures_;
  const bool require_connectivity_;
  const std::string pref_name_;
  bool is_initialized_ = false;
  std::string connection_listener_name_;

  std::unique_ptr<nearby::Timer> timer_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_BASE_H_
