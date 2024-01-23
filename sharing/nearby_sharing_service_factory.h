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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_FACTORY_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_FACTORY_H_

#include <memory>

#include "internal/analytics/event_logger.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/public/context.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_sharing_decoder.h"
#include "sharing/nearby_sharing_service.h"

namespace nearby {
namespace sharing {

class NearbySharingServiceFactory {
 public:
  enum class LinkType { kStatic, kDynamic };

  // Return a singleton instance of NearbySharingServiceFactory.
  static NearbySharingServiceFactory* GetInstance();

  NearbySharingService* CreateSharingService(
      LinkType link_type,
      nearby::sharing::api::SharingPlatform& sharing_platform,
      std::unique_ptr<::nearby::analytics::EventLogger> event_logger = nullptr);

 private:
  NearbySharingServiceFactory() = default;

  std::unique_ptr<Context> context_;
  std::unique_ptr<::nearby::analytics::EventLogger> event_logger_;
  std::unique_ptr<NearbySharingDecoder> decoder_;
  std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  std::unique_ptr<NearbySharingService> nearby_sharing_service_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_FACTORY_H_
