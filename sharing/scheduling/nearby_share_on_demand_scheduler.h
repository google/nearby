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

#ifndef THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_ON_DEMAND_SCHEDULER_H_
#define THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_ON_DEMAND_SCHEDULER_H_

#include <optional>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/scheduling/nearby_share_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler_base.h"

namespace nearby {
namespace sharing {

// A NearbyShareSchedulerBase that does not schedule recurring tasks.
class NearbyShareOnDemandScheduler : public NearbyShareSchedulerBase {
 public:
  // See NearbyShareSchedulerBase for a description of inputs.
  NearbyShareOnDemandScheduler(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      bool retry_failures, bool require_connectivity,
      absl::string_view pref_name, OnRequestCallback callback);

  ~NearbyShareOnDemandScheduler() override;

 private:
  // Return absl::nullopt so as not to schedule recurring requests.
  std::optional<absl::Duration> TimeUntilRecurringRequest(
      absl::Time now) const override;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_ON_DEMAND_SCHEDULER_H_
