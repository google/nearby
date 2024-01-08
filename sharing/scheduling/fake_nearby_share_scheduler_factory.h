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

#ifndef THIRD_PARTY_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_FACTORY_H_
#define THIRD_PARTY_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_FACTORY_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/scheduling/fake_nearby_share_scheduler.h"
#include "sharing/scheduling/nearby_share_expiration_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler_factory.h"

namespace nearby {
namespace sharing {

// A fake NearbyShareScheduler factory that creates instances of
// FakeNearbyShareScheduler instead of expiration, on-demand, or periodic
// scheduler. It stores the factory input parameters as well as a raw pointer to
// the fake scheduler for each instance created.
class FakeNearbyShareSchedulerFactory : public NearbyShareSchedulerFactory {
 public:
  struct ExpirationInstance {
    ExpirationInstance(
        nearby::sharing::api::PreferenceManager& pref_manager,
        const nearby::Clock* c);
    ExpirationInstance(ExpirationInstance&&);
    ~ExpirationInstance();

    FakeNearbyShareScheduler* fake_scheduler = nullptr;
    NearbyShareExpirationScheduler::ExpirationTimeFunctor
        expiration_time_functor;
    bool retry_failures;
    bool require_connectivity;
    nearby::sharing::api::PreferenceManager& preference_manager;
    const nearby::Clock* const clock;
  };

  struct OnDemandInstance {
    OnDemandInstance(
        nearby::sharing::api::PreferenceManager& pref_manager,
        const nearby::Clock* c);
    FakeNearbyShareScheduler* fake_scheduler = nullptr;
    bool retry_failures;
    bool require_connectivity;
    nearby::sharing::api::PreferenceManager& preference_manager;
    const nearby::Clock* const clock;
  };

  struct PeriodicInstance {
    PeriodicInstance(
        nearby::sharing::api::PreferenceManager& pref_manager,
        const nearby::Clock* c);
    FakeNearbyShareScheduler* fake_scheduler = nullptr;
    absl::Duration request_period;
    bool retry_failures;
    bool require_connectivity;
    nearby::sharing::api::PreferenceManager& preference_manager;
    const nearby::Clock* const clock;
  };

  FakeNearbyShareSchedulerFactory();
  ~FakeNearbyShareSchedulerFactory() override;

  const absl::flat_hash_map<std::string, ExpirationInstance>&
  pref_name_to_expiration_instance() const {
    return pref_name_to_expiration_instance_;
  }

  const absl::flat_hash_map<std::string, OnDemandInstance>&
  pref_name_to_on_demand_instance() const {
    return pref_name_to_on_demand_instance_;
  }

  const absl::flat_hash_map<std::string, PeriodicInstance>&
  pref_name_to_periodic_instance() const {
    return pref_name_to_periodic_instance_;
  }

 private:
  // NearbyShareSchedulerFactory:
  std::unique_ptr<NearbyShareScheduler> CreateExpirationSchedulerInstance(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      NearbyShareExpirationScheduler::ExpirationTimeFunctor
          expiration_time_functor,
      bool retry_failures, bool require_connectivity,
      absl::string_view pref_name,
      NearbyShareScheduler::OnRequestCallback on_request_callback) override;

  std::unique_ptr<NearbyShareScheduler> CreateOnDemandSchedulerInstance(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      bool retry_failures, bool require_connectivity,
      absl::string_view pref_name,
      NearbyShareScheduler::OnRequestCallback callback) override;

  std::unique_ptr<NearbyShareScheduler> CreatePeriodicSchedulerInstance(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      absl::Duration request_period, bool retry_failures,
      bool require_connectivity, absl::string_view pref_name,
      NearbyShareScheduler::OnRequestCallback callback) override;

  absl::flat_hash_map<std::string, ExpirationInstance>
      pref_name_to_expiration_instance_;
  absl::flat_hash_map<std::string, OnDemandInstance>
      pref_name_to_on_demand_instance_;
  absl::flat_hash_map<std::string, PeriodicInstance>
      pref_name_to_periodic_instance_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_SCHEDULING_FAKE_NEARBY_SHARE_SCHEDULER_FACTORY_H_
