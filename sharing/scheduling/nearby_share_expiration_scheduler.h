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

#ifndef THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_EXPIRATION_SCHEDULER_H_
#define THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_EXPIRATION_SCHEDULER_H_

#include <functional>
#include <optional>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/scheduling/nearby_share_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler_base.h"

namespace nearby {
namespace sharing {

// A NearbyShareSchedulerBase that schedules recurring tasks based on an
// expiration time provided by the owner.
class NearbyShareExpirationScheduler : public NearbyShareSchedulerBase {
 public:
  using ExpirationTimeFunctor = std::function<std::optional<absl::Time>()>;

  // |expiration_time_functor|: A function provided by the owner that returns
  //     the next expiration time.
  // See NearbyShareSchedulerBase for a description of other inputs.
  NearbyShareExpirationScheduler(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      ExpirationTimeFunctor expiration_time_functor, bool retry_failures,
      bool require_connectivity, absl::string_view pref_name,
      OnRequestCallback on_request_callback);

  ~NearbyShareExpirationScheduler() override;

 protected:
  std::optional<absl::Duration> TimeUntilRecurringRequest(
      absl::Time now) const override;

  ExpirationTimeFunctor expiration_time_functor_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_EXPIRATION_SCHEDULER_H_
