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

#include "sharing/scheduling/nearby_share_on_demand_scheduler.h"

#include <optional>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/scheduling/nearby_share_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler_base.h"

namespace nearby {
namespace sharing {
using ::nearby::sharing::api::PreferenceManager;

NearbyShareOnDemandScheduler::NearbyShareOnDemandScheduler(
    Context* context, PreferenceManager& preference_manager,
    bool retry_failures, bool require_connectivity, absl::string_view pref_name,
    OnRequestCallback callback)
    : NearbyShareSchedulerBase(context, preference_manager, retry_failures,
                               require_connectivity, pref_name,
                               std::move(callback)) {}

NearbyShareOnDemandScheduler::~NearbyShareOnDemandScheduler() = default;

std::optional<absl::Duration>
NearbyShareOnDemandScheduler::TimeUntilRecurringRequest(absl::Time now) const {
  return std::nullopt;
}

}  // namespace sharing
}  // namespace nearby
