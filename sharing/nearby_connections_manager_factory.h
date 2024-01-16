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

#include "internal/analytics/event_logger.h"
#include "internal/platform/device_info.h"
#include "sharing/internal/public/context.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_sharing_service_factory.h"

namespace nearby {
namespace sharing {

class NearbyConnectionsManagerFactory {
 public:
  using LinkType = NearbySharingServiceFactory::LinkType;

  // Return a singleton instance of NearbyConnectionsManagerFactory.
  static std::unique_ptr<NearbyConnectionsManager> CreateConnectionsManager(
      NearbySharingServiceFactory::LinkType link_type, Context* context,
      nearby::DeviceInfo& device_info,
      nearby::analytics::EventLogger* event_logger = nullptr);

 private:
  NearbyConnectionsManagerFactory() = default;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_FACTORY_H_
