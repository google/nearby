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

#include "internal/analytics/event_logger.h"
#include "internal/platform/device_info.h"
#include "sharing/internal/public/context.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_manager_impl.h"
#include "sharing/nearby_connections_service_impl.h"

namespace nearby {
namespace sharing {

std::unique_ptr<NearbyConnectionsManager>
NearbyConnectionsManagerFactory::CreateConnectionsManager(
    LinkType link_type, Context* context,
    nearby::DeviceInfo& device_info,
    nearby::analytics::EventLogger* event_logger) {
  return std::make_unique<NearbyConnectionsManagerImpl>(
      context, *context->GetConnectivityManager(), device_info,
      std::make_unique<NearbyConnectionsServiceImpl>(event_logger));
}

}  // namespace sharing
}  // namespace nearby
