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

#ifndef CORE_INTERNAL_BLUETOOTH_BWU_HANDLER_H_
#define CORE_INTERNAL_BLUETOOTH_BWU_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/bluetooth_classic.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/medium_selector.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {

// Defines the set of methods that need to be implemented to handle the
// per-Medium-specific operations needed to upgrade an EndpointChannel.
class BluetoothBwuHandler : public BaseBwuHandler {
 public:
  explicit BluetoothBwuHandler(
      Mediums& mediums,
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
  std::pair<std::unique_ptr<EndpointChannel>,
            location::nearby::proto::connections::OperationResultCode>
  CreateUpgradedEndpointChannel(ClientProxy* client,
                                const std::string& service_id,
                                const std::string& endpoint_id,
                                const UpgradePathInfo& upgrade_path_info) final;
  Medium GetUpgradeMedium() const final { return Medium::BLUETOOTH; }
  void OnEndpointDisconnect(ClientProxy* client,
                            const std::string& endpoint_id) final {}

  // BaseBwuHandler implementation:
  ByteArray HandleInitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& upgrade_service_id,
      const std::string& endpoint_id) final;
  void HandleRevertInitiatorStateForService(
      const std::string& upgrade_service_id) final;

  void OnIncomingBluetoothConnection(ClientProxy* client,
                                     const std::string& upgrade_service_id,
                                     BluetoothSocket socket);

  Mediums& mediums_;
  BluetoothRadio& bluetooth_radio_{mediums_.GetBluetoothRadio()};
  BluetoothClassic& bluetooth_medium_{mediums_.GetBluetoothClassic()};
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_BLUETOOTH_BWU_HANDLER_H_
