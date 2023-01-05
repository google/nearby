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
    upgrade_service_id_to_active_endpoint_ids_[upgrade_service_id].insert(
        endpoint_id);
  }

  return upgrade_path_available_frame;
}

void BaseBwuHandler::RevertInitiatorState() {
  for (const auto& pair : upgrade_service_id_to_active_endpoint_ids_) {
    HandleRevertInitiatorStateForService(pair.first);
  }
  upgrade_service_id_to_active_endpoint_ids_.clear();
}

void BaseBwuHandler::RevertInitiatorState(const std::string& upgrade_service_id,
                                          const std::string& endpoint_id) {
  if (!IsInitiatorUpgradeServiceId(upgrade_service_id)) {
    NEARBY_LOGS(ERROR)
        << "BaseBwuHandler::RevertInitiatorState: input service ID "
        << upgrade_service_id << " is not an BWU initiator ID; ignoring.";
    return;
  }

  auto it = upgrade_service_id_to_active_endpoint_ids_.find(upgrade_service_id);
  if (it == upgrade_service_id_to_active_endpoint_ids_.end() ||
      it->second.empty()) {
    return;
  }

  // If the last endpoint for the service has been reverted, alert the specific
  // medium handler (child class) to perform any clean-up .
  it->second.erase(endpoint_id);
  if (it->second.empty()) {
    upgrade_service_id_to_active_endpoint_ids_.erase(it);
    HandleRevertInitiatorStateForService(upgrade_service_id);
  }
}

void BaseBwuHandler::RevertResponderState(const std::string& service_id) {
  HandleRevertInitiatorStateForService(service_id);
}

}  // namespace connections
}  // namespace nearby
