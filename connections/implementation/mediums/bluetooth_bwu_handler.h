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

#ifndef CORE_INTERNAL_MEDIUMS_BLUETOOTH_BWU_HANDLER_H_
#define CORE_INTERNAL_MEDIUMS_BLUETOOTH_BWU_HANDLER_H_

#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/bluetooth_classic.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/medium_selector.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/expected.h"

namespace nearby {
namespace connections {

// Defines the set of methods that need to be implemented to handle the
// per-Medium-specific operations needed to upgrade an EndpointChannel.
class BluetoothBwuHandler : public BaseBwuHandler {
 public:
  BluetoothBwuHandler(
      BluetoothRadio* absl_nonnull bluetooth_radio,
      BluetoothClassic* absl_nonnull bluetooth_medium,
      IncomingConnectionCallback incoming_connection_callback);

 private:
  class BluetoothIncomingSocket : public IncomingSocket {
   public:
    explicit BluetoothIncomingSocket(const std::string& name,
                                     BluetoothSocket socket)
        : name_(name), socket_(socket) {}

    std::string ToString() override { return name_; }
    void Close() override { socket_.Close(); }

   private:
    std::string name_;
    BluetoothSocket socket_;
  };

  // BwuHandler implementation:
  ErrorOr<std::unique_ptr<EndpointChannel>> CreateUpgradedEndpointChannel(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id,
      const location::nearby::connections::BandwidthUpgradeNegotiationFrame::
          UpgradePathInfo& upgrade_path_info) final;
  Medium GetUpgradeMedium() const final { return Medium::BLUETOOTH; }
  void OnEndpointDisconnect(ClientProxy* client,
                            const std::string& endpoint_id) final {}

  // BaseBwuHandler implementation:
  std::string HandleInitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& upgrade_service_id,
      const std::string& endpoint_id) final;
  void HandleRevertInitiatorStateForService(
      const std::string& upgrade_service_id) final;

  void OnIncomingBluetoothConnection(ClientProxy* client,
                                     const std::string& upgrade_service_id,
                                     BluetoothSocket socket);

  BluetoothRadio& bluetooth_radio_;
  BluetoothClassic& bluetooth_medium_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLUETOOTH_BWU_HANDLER_H_
