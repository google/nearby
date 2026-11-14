// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_FACTORY_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_FACTORY_H_

#include <memory>

#include "location/nearby/analytics/cpp/logging/event_logger.h"
#include "absl/base/nullability.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/single_thread_executor.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/nearby_connections_manager.h"

namespace nearby::sharing {

class NearbyConnectionsManagerFactory {
 public:
  // Return a singleton instance of NearbyConnectionsManagerFactory.
  // |connections_callback_task_runner| is the executor where callbacks from
  // NearbyConnection destined for NearbySharingService need to be scheduled.
  // This is usually the thread that NearbySharingService is running on.
  static std::unique_ptr<NearbyConnectionsManager> CreateConnectionsManager(
      nearby::SingleThreadExecutor* absl_nonnull
          connections_callback_task_runner,
      ConnectivityManager* absl_nonnull connectivity_manager,
      nearby::api::DeviceInfo& device_info,
      nearby::analytics::EventLogger* event_logger = nullptr);

 private:
  NearbyConnectionsManagerFactory() = default;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_FACTORY_H_
