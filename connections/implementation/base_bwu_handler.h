// Copyright 2020 Google LLC
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

#ifndef CORE_INTERNAL_BASE_BWU_HANDLER_H_
#define CORE_INTERNAL_BASE_BWU_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/time/clock.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/scheduled_executor.h"
#include "internal/platform/single_thread_executor.h"

namespace location {
namespace nearby {
namespace connections {

class BaseBwuHandler : public BwuHandler {
 public:
  using ClientIntroduction = BwuNegotiationFrame::ClientIntroduction;

  BaseBwuHandler(EndpointChannelManager& channel_manager,
                 BwuNotifications bwu_notifications)
      : channel_manager_(&channel_manager),
        bwu_notifications_(std::move(bwu_notifications)) {}
  ~BaseBwuHandler() override = default;

 protected:
  // Represents the incoming Socket the Initiator has gotten after initializing
  // its upgraded bandwidth medium.
  EndpointChannelManager* GetEndpointChannelManager();
  EndpointChannelManager* channel_manager_;
  BwuNotifications bwu_notifications_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_BASE_BWU_HANDLER_H_
