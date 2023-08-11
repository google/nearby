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

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/endpoint_channel_manager.h"

namespace nearby {
namespace connections {

// Manages the bookkeeping common to all medium handlers. Notably, it tracks all
// of the service IDs and endpoint IDs that initiated a bandwidth upgrade.
class BaseBwuHandler : public BwuHandler {
 public:
  explicit BaseBwuHandler(
      IncomingConnectionCallback incoming_connection_callback);

  // BwuHandler implementation:
  ByteArray InitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id) final;
  void RevertInitiatorState() final;
  void RevertInitiatorState(const std::string& upgrade_service_id,
                            const std::string& endpoint_id) final;
  // If BWU Medium is Hotspot. The client needs to disconnect from Hotspot, then
  // it can restore the previous AP connection right away. The following method
  // is only for Hotspot Client
  void RevertResponderState(const std::string& service_id) final;

 protected:
  // Invoked by InitializeUpgradedMediumForEndpoint and RevertInitiatorState,
  // respectively, to handle medium-specific logic.
  // HandleRevertInitiatorStateForService is only invoked after the last
  // endpoint for the service is reverted.
  virtual ByteArray HandleInitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& upgrade_service_id,
      const std::string& endpoint_id) = 0;
  virtual void HandleRevertInitiatorStateForService(
      const std::string& upgrade_service_id) = 0;

  // Notifies the caller about incoming connection.
  void NotifyOnIncomingConnection(
      ClientProxy* client,
      std::unique_ptr<IncomingSocketConnection> connection);

 private:
  IncomingConnectionCallback incoming_connection_callback_;
  // Map from the (wrapped) service ID to endpoint IDs that are initiating a
  // bandwidth upgrade. Not used for endpoints that respond to bandwidth upgrade
  // requests from another device.
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>
      upgrade_service_id_to_active_endpoint_ids_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_BASE_BWU_HANDLER_H_
