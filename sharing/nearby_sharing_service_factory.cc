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

#include "sharing/nearby_sharing_service_factory.h"

#include <memory>
#include <utility>

#include "internal/analytics/event_logger.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/public/context_impl.h"
#include "sharing/nearby_connections_manager_factory.h"
#include "sharing/nearby_sharing_decoder_impl.h"
#include "sharing/nearby_sharing_event_logger.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_service_impl.h"

namespace nearby {
namespace sharing {

using ::nearby::sharing::api::SharingPlatform;

NearbySharingServiceFactory* NearbySharingServiceFactory::GetInstance() {
  static NearbySharingServiceFactory* instance =
      new NearbySharingServiceFactory();
  return instance;
}

NearbySharingService* NearbySharingServiceFactory::CreateSharingService(
    LinkType link_type,
    SharingPlatform& sharing_platform,
    std::unique_ptr<::nearby::analytics::EventLogger> event_logger) {
  if (nearby_sharing_service_ != nullptr) {
    return nullptr;
  }

  context_ =
      std::make_unique<ContextImpl>(sharing_platform);
  event_logger_ = std::make_unique<NearbySharingEventLogger>(
      sharing_platform.GetPreferenceManager(),
      std::move(event_logger));
  decoder_ = std::make_unique<NearbySharingDecoderImpl>();
  nearby_connections_manager_ =
      NearbyConnectionsManagerFactory::CreateConnectionsManager(
          link_type, context_.get(), sharing_platform.GetDeviceInfo(),
          event_logger_.get());

  nearby_sharing_service_ = std::make_unique<NearbySharingServiceImpl>(
      context_.get(), sharing_platform, decoder_.get(),
      std::move(nearby_connections_manager_), event_logger_.get());

  return nearby_sharing_service_.get();
}

}  // namespace sharing
}  // namespace nearby
