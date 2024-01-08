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

#include "sharing/scheduling/nearby_share_scheduler_factory.h"

#include <memory>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/scheduling/nearby_share_expiration_scheduler.h"
#include "sharing/scheduling/nearby_share_on_demand_scheduler.h"
#include "sharing/scheduling/nearby_share_periodic_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler.h"

namespace nearby {
namespace sharing {
using ::nearby::sharing::api::PreferenceManager;

// static
NearbyShareSchedulerFactory* NearbyShareSchedulerFactory::test_factory_ =
    nullptr;

// static
std::unique_ptr<NearbyShareScheduler>
NearbyShareSchedulerFactory::CreateExpirationScheduler(
    Context* context, PreferenceManager& preference_manager,
    NearbyShareExpirationScheduler::ExpirationTimeFunctor
        expiration_time_functor,
    bool retry_failures, bool require_connectivity, absl::string_view pref_name,
    NearbyShareScheduler::OnRequestCallback on_request_callback) {
  if (test_factory_) {
    return test_factory_->CreateExpirationSchedulerInstance(
        context, preference_manager, std::move(expiration_time_functor),
        retry_failures, require_connectivity, pref_name,
        std::move(on_request_callback));
  }

  return std::make_unique<NearbyShareExpirationScheduler>(
      context, preference_manager, std::move(expiration_time_functor),
      retry_failures, require_connectivity, pref_name,
      std::move(on_request_callback));
}

// static
std::unique_ptr<NearbyShareScheduler>
NearbyShareSchedulerFactory::CreateOnDemandScheduler(
    Context* context, PreferenceManager& preference_manager,
    bool retry_failures, bool require_connectivity, absl::string_view pref_name,
    NearbyShareScheduler::OnRequestCallback callback) {
  if (test_factory_) {
    return test_factory_->CreateOnDemandSchedulerInstance(
        context, preference_manager, retry_failures, require_connectivity,
        pref_name, std::move(callback));
  }

  return std::make_unique<NearbyShareOnDemandScheduler>(
      context, preference_manager, retry_failures, require_connectivity,
      pref_name, std::move(callback));
}

// static
std::unique_ptr<NearbyShareScheduler>
NearbyShareSchedulerFactory::CreatePeriodicScheduler(
    Context* context, PreferenceManager& preference_manager,
    absl::Duration request_period, bool retry_failures,
    bool require_connectivity, absl::string_view pref_name,
    NearbyShareScheduler::OnRequestCallback callback) {
  if (test_factory_) {
    return test_factory_->CreatePeriodicSchedulerInstance(
        context, preference_manager, request_period, retry_failures,
        require_connectivity, pref_name, std::move(callback));
  }

  return std::make_unique<NearbySharePeriodicScheduler>(
      context, preference_manager, request_period, retry_failures,
      require_connectivity, pref_name, std::move(callback));
}

// static
void NearbyShareSchedulerFactory::SetFactoryForTesting(
    NearbyShareSchedulerFactory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareSchedulerFactory::~NearbyShareSchedulerFactory() = default;

}  // namespace sharing
}  // namespace nearby
