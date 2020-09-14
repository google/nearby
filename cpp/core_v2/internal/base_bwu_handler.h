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

#ifndef CORE_V2_INTERNAL_BASE_BWU_HANDLER_H_
#define CORE_V2_INTERNAL_BASE_BWU_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "core_v2/internal/bwu_handler.h"
#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/endpoint_channel_manager.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform_v2/public/cancelable_alarm.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/scheduled_executor.h"
#include "platform_v2/public/single_thread_executor.h"
#include "proto/connections_enums.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/time/clock.h"

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
  void OnIncomingConnection(ClientProxy* client,
                            IncomingSocketConnection* connection);

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

#endif  // CORE_V2_INTERNAL_BASE_BWU_HANDLER_H_
