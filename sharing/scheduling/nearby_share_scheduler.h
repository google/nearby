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

#ifndef THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_H_
#define THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_H_

#include <stddef.h>

#include <functional>
#include <optional>

#include "absl/time/time.h"

namespace nearby {
namespace sharing {

// Schedules tasks and alerts the owner when a request is ready. Scheduling
// begins after Start() is called, and scheduling is stopped via Stop().
//
// An immediate request can be made, bypassing any current scheduling, via
// MakeImmediateRequest(). When a request attempt has completed--successfully
// or not--the owner should invoke HandleResult() so the scheduler can process
// the attempt outcomes and schedule future attempts if necessary.
class NearbyShareScheduler {
 public:
  using OnRequestCallback = std::function<void()>;

  explicit NearbyShareScheduler(OnRequestCallback callback);
  virtual ~NearbyShareScheduler();

  void Start();
  void Stop();
  bool is_running() const { return is_running_; }

  // Make a request that runs as soon as possible.
  virtual void MakeImmediateRequest() = 0;

  // Processes the result of the previous request. Method to be called by the
  // owner when the request is finished. The timer for the next request is
  // automatically scheduled.
  virtual void HandleResult(bool success) = 0;

  // Recomputes the time until the next request, using GetTimeUntilNextRequest()
  // as the source of truth. This method is essentially idempotent. NOTE: This
  // method should rarely need to be called.
  virtual void Reschedule() = 0;

  // Returns the time of the last known successful request. If no request has
  // succeeded, absl::nullopt is returned.
  virtual std::optional<absl::Time> GetLastSuccessTime() const = 0;

  // Returns the time until the next scheduled request. Returns std::nullopt if
  // there is no request scheduled.
  virtual std::optional<absl::Duration> GetTimeUntilNextRequest() const = 0;

  // Returns true after the |callback_| has been alerted of a request but before
  // HandleResult() is invoked.
  virtual bool IsWaitingForResult() const = 0;

  // The number of times the current request type has failed.
  // Once the request succeeds or a fresh request is made--for example,
  // via a manual request--this counter is reset.
  virtual size_t GetNumConsecutiveFailures() const = 0;

 protected:
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  // Invokes |callback_|, alerting the owner that a request is ready.
  void NotifyOfRequest();

 private:
  bool is_running_ = false;
  OnRequestCallback callback_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_H_
