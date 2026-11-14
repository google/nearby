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

#include "sharing/nearby_connections_manager_factory.h"

#include <memory>

#include "location/nearby/analytics/cpp/logging/event_logger.h"
#include "absl/base/nullability.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/task_runner.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_manager_impl.h"
#include "sharing/nearby_connections_service_impl.h"

namespace nearby::sharing {

std::unique_ptr<NearbyConnectionsManager>
NearbyConnectionsManagerFactory::CreateConnectionsManager(
    nearby::TaskRunner* absl_nonnull connections_callback_task_runner,
    ConnectivityManager* absl_nonnull connectivity_manager,
    nearby::api::DeviceInfo& device_info,
    nearby::analytics::EventLogger* event_logger) {
  return std::make_unique<NearbyConnectionsManagerImpl>(
      connections_callback_task_runner, *connectivity_manager, device_info,
      std::make_unique<NearbyConnectionsServiceImpl>(connectivity_manager,
                                                     event_logger));
}

}  // namespace nearby::sharing
