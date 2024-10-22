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

#include "sharing/scheduling/fake_nearby_share_scheduler_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/clock.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/scheduling/fake_nearby_share_scheduler.h"
#include "sharing/scheduling/nearby_share_expiration_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler.h"

namespace nearby {
namespace sharing {

using ::nearby::sharing::api::PreferenceManager;

FakeNearbyShareSchedulerFactory::ExpirationInstance::ExpirationInstance(
    PreferenceManager& pref_manager, const nearby::Clock* c)
    : preference_manager(pref_manager),
      clock(c) {}

FakeNearbyShareSchedulerFactory::ExpirationInstance::ExpirationInstance(
    ExpirationInstance&&) = default;

FakeNearbyShareSchedulerFactory::ExpirationInstance::~ExpirationInstance() =
    default;

FakeNearbyShareSchedulerFactory::OnDemandInstance::OnDemandInstance(
    PreferenceManager& pref_manager, const nearby::Clock* c)
    : preference_manager(pref_manager),
      clock(c) {}

FakeNearbyShareSchedulerFactory::PeriodicInstance::PeriodicInstance(
    PreferenceManager& pref_manager, const nearby::Clock* c)
    : preference_manager(pref_manager),
      clock(c) {}

FakeNearbyShareSchedulerFactory::FakeNearbyShareSchedulerFactory() = default;

FakeNearbyShareSchedulerFactory::~FakeNearbyShareSchedulerFactory() = default;

std::unique_ptr<NearbyShareScheduler>
FakeNearbyShareSchedulerFactory::CreateExpirationSchedulerInstance(
    Context* context, PreferenceManager& preference_manager,
    NearbyShareExpirationScheduler::ExpirationTimeFunctor
        expiration_time_functor,
    bool retry_failures, bool require_connectivity, absl::string_view pref_name,
    NearbyShareScheduler::OnRequestCallback on_request_callback) {
  ExpirationInstance instance(preference_manager, context->GetClock());
  instance.expiration_time_functor = std::move(expiration_time_functor);
  instance.retry_failures = retry_failures;
  instance.require_connectivity = require_connectivity;

  auto scheduler = std::make_unique<FakeNearbyShareScheduler>(
      std::move(on_request_callback));
  instance.fake_scheduler = scheduler.get();

  pref_name_to_expiration_instance_.erase(pref_name);
  pref_name_to_expiration_instance_.emplace(pref_name, std::move(instance));

  return scheduler;
}

std::unique_ptr<NearbyShareScheduler>
FakeNearbyShareSchedulerFactory::CreateOnDemandSchedulerInstance(
    Context* context, PreferenceManager& preference_manager,
    bool retry_failures, bool require_connectivity, absl::string_view pref_name,
    NearbyShareScheduler::OnRequestCallback callback) {
  OnDemandInstance instance(preference_manager, context->GetClock());
  instance.retry_failures = retry_failures;
  instance.require_connectivity = require_connectivity;

  auto scheduler =
      std::make_unique<FakeNearbyShareScheduler>(std::move(callback));
  instance.fake_scheduler = scheduler.get();

  pref_name_to_on_demand_instance_.erase(pref_name);
  pref_name_to_on_demand_instance_.emplace(pref_name, instance);

  return scheduler;
}

std::unique_ptr<NearbyShareScheduler>
FakeNearbyShareSchedulerFactory::CreatePeriodicSchedulerInstance(
    Context* context, PreferenceManager& preference_manager,
    absl::Duration request_period, bool retry_failures,
    bool require_connectivity, absl::string_view pref_name,
    NearbyShareScheduler::OnRequestCallback callback) {
  PeriodicInstance instance(preference_manager, context->GetClock());
  instance.request_period = request_period;
  instance.retry_failures = retry_failures;
  instance.require_connectivity = require_connectivity;

  auto scheduler =
      std::make_unique<FakeNearbyShareScheduler>(std::move(callback));
  instance.fake_scheduler = scheduler.get();

  pref_name_to_periodic_instance_.erase(pref_name);
  pref_name_to_periodic_instance_.emplace(pref_name, instance);

  return scheduler;
}

}  // namespace sharing
}  // namespace nearby
