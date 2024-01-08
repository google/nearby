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

#ifndef THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_FACTORY_H_
#define THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_FACTORY_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/scheduling/nearby_share_expiration_scheduler.h"
#include "sharing/scheduling/nearby_share_scheduler.h"

namespace nearby {
namespace sharing {

// class PrefService;

// Used to create instances of NearbyShareExpirationScheduler,
// NearbyShareOnDemandScheduler, and NearbySharePeriodicScheduler. A fake
// factory can also be set for testing purposes.
class NearbyShareSchedulerFactory {
 public:
  static std::unique_ptr<NearbyShareScheduler> CreateExpirationScheduler(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      NearbyShareExpirationScheduler::ExpirationTimeFunctor
          expiration_time_functor,
      bool retry_failures, bool require_connectivity,
      absl::string_view pref_name,
      NearbyShareScheduler::OnRequestCallback on_request_callback);

  static std::unique_ptr<NearbyShareScheduler> CreateOnDemandScheduler(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      bool retry_failures, bool require_connectivity,
      absl::string_view pref_name,
      NearbyShareScheduler::OnRequestCallback callback);

  static std::unique_ptr<NearbyShareScheduler> CreatePeriodicScheduler(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      absl::Duration request_period, bool retry_failures,
      bool require_connectivity, absl::string_view pref_name,
      NearbyShareScheduler::OnRequestCallback callback);

  static void SetFactoryForTesting(NearbyShareSchedulerFactory* test_factory);

 protected:
  virtual ~NearbyShareSchedulerFactory();

  virtual std::unique_ptr<NearbyShareScheduler>
  CreateExpirationSchedulerInstance(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      NearbyShareExpirationScheduler::ExpirationTimeFunctor
          expiration_time_functor,
      bool retry_failures, bool require_connectivity,
      absl::string_view pref_name,
      NearbyShareScheduler::OnRequestCallback on_request_callback) = 0;

  virtual std::unique_ptr<NearbyShareScheduler> CreateOnDemandSchedulerInstance(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      bool retry_failures, bool require_connectivity,
      absl::string_view pref_name,
      NearbyShareScheduler::OnRequestCallback callback) = 0;

  virtual std::unique_ptr<NearbyShareScheduler> CreatePeriodicSchedulerInstance(
      Context* context,
      nearby::sharing::api::PreferenceManager& preference_manager,
      absl::Duration request_period, bool retry_failures,
      bool require_connectivity, absl::string_view pref_name,
      NearbyShareScheduler::OnRequestCallback callback) = 0;

 private:
  static NearbyShareSchedulerFactory* test_factory_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_SCHEDULING_NEARBY_SHARE_SCHEDULER_FACTORY_H_
