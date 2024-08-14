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

#include <cstdint>
#include <memory>

#include "internal/analytics/event_logger.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/public/context.h"
#include "sharing/nearby_sharing_service.h"

namespace nearby::sharing {

class NearbySharingServiceFactory {
 public:
  // Return a singleton instance of NearbySharingServiceFactory.
  static NearbySharingServiceFactory* GetInstance();

  NearbySharingService* CreateSharingService(
      int32_t vendor_id,
      nearby::sharing::api::SharingPlatform& sharing_platform,
      ::nearby::analytics::EventLogger* event_logger);

 private:
  NearbySharingServiceFactory() = default;

  std::unique_ptr<Context> context_;
  ::nearby::analytics::EventLogger* event_logger_ = nullptr;
  std::unique_ptr<NearbySharingService> nearby_sharing_service_;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_FACTORY_H_
