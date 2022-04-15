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

#include "connections/implementation/base_bwu_handler.h"

#include <string>
#include <utility>

#include "connections/implementation/service_id_constants.h"
#include "internal/platform/logging.h"

namespace location {
namespace nearby {
namespace connections {

BaseBwuHandler::BaseBwuHandler(BwuNotifications bwu_notifications)
    : bwu_notifications_(std::move(bwu_notifications)) {}

ByteArray BaseBwuHandler::InitializeUpgradedMediumForEndpoint(
    ClientProxy* client, const std::string& service_id,
    const std::string& endpoint_id) {
  std::string upgrade_service_id = WrapInitiatorUpgradeServiceId(service_id);

  // Perform any medium-specific handling in the child class.
  ByteArray upgrade_path_available_frame =
      HandleInitializeUpgradedMediumForEndpoint(client, upgrade_service_id,
                                                endpoint_id);
  if (!upgrade_path_available_frame.Empty()) {
    active_service_ids_.insert(upgrade_service_id);
  }

  return upgrade_path_available_frame;
}

void BaseBwuHandler::RevertInitiatorState() {
  for (const auto& service_id : active_service_ids_) {
    HandleRevertInitiatorStateForService(service_id);
  }
  active_service_ids_.clear();
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
