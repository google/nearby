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

#ifndef NEARBY_CONNECTIONS_IMPLEMENTATION_FAKE_BWU_HANDLER_H_
#define NEARBY_CONNECTIONS_IMPLEMENTATION_FAKE_BWU_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "connections/implementation/base_bwu_handler.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/fake_endpoint_channel.h"

namespace nearby {
namespace connections {

// Fake implementation of a bandwidth-upgrade handler used for testing. The
// service and endpoint IDs inputs are recorded and can be inspected. The method
// NotifyBwuManagerOfIncomingConnection will send the BwuManager an incoming
// connection corresponding to a previous
// HandleInitializeUpgradedMediumForEndpoint call.
class FakeBwuHandler : public BaseBwuHandler {
 public:
  using Medium = ::location::nearby::proto::connections::Medium;

  // The arguments passed to the BwuHandler methods. Not all values are set for
  // every method.
  struct InputData {
    ClientProxy* client = nullptr;
    std::optional<std::string> service_id;
    std::optional<std::string> endpoint_id;
  };

  explicit FakeBwuHandler(Medium medium)
      : BaseBwuHandler(nullptr), medium_(medium) {}
  ~FakeBwuHandler() override = default;

  const std::vector<InputData>& create_calls() const { return create_calls_; }
  const std::vector<InputData>& disconnect_calls() const {
    return disconnect_calls_;
  }
  const std::vector<InputData>& handle_initialize_calls() const {
    return handle_initialize_calls_;
  }
  const std::vector<InputData>& handle_revert_calls() const {
    return handle_revert_calls_;
  }

  // Builds an incoming connection corresponding to
  // handle_initialize_calls()[initialize_call_index], and sends it to the
  // BwuManager. Return a pointer to the upgraded channel.
  FakeEndpointChannel* NotifyBwuManagerOfIncomingConnection(
      size_t initialize_call_index, BwuManager* bwu_manager) {
    CHECK_GT(handle_initialize_calls_.size(), initialize_call_index);

    // Simulate establishing a connection with the remote device (BWU
    // Responder). Also load a valid introduction frame in the new channel that
    // will be read by the local device (BWU Initiator). This simulates the
    // remote device (BWU Responder) writing the
    // BANDWIDTH_UPGRADE_NEGOTIATION.CLIENT_INTRODUCTION.
    auto upgraded_channel = std::make_unique<FakeEndpointChannel>(
        medium_, *handle_initialize_calls_[initialize_call_index].service_id);
    FakeEndpointChannel* upgraded_channel_raw = upgraded_channel.get();
    upgraded_channel->set_read_output(
        ExceptionOr<ByteArray>(parser::ForBwuIntroduction(
            *handle_initialize_calls_[initialize_call_index].endpoint_id,
            false /* supports_disabling_encryption */)));
    auto connection = std::make_unique<IncomingSocketConnection>();
    connection->channel = std::move(upgraded_channel);

    bwu_manager->InvokeOnIncomingConnectionForTesting(
        handle_initialize_calls_[initialize_call_index].client,
        std::move(connection));

    return upgraded_channel_raw;
  }

 private:
  class FakeIncomingSocket : public BwuHandler::IncomingSocket {
   public:
    explicit FakeIncomingSocket(const std::string& name) : name_(name) {}
    ~FakeIncomingSocket() override = default;

    std::string ToString() override { return name_; }
    void Close() override {}

   private:
    std::string name_;
  };

  // BwuHandler:
  std::unique_ptr<EndpointChannel> CreateUpgradedEndpointChannel(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id,
      const UpgradePathInfo& upgrade_path_info) final {
    create_calls_.push_back({.client = client,
                             .service_id = service_id,
                             .endpoint_id = endpoint_id});
    return std::make_unique<FakeEndpointChannel>(medium_, service_id);
  }

  Medium GetUpgradeMedium() const final { return medium_; }

  void OnEndpointDisconnect(ClientProxy* client,
                            const std::string& endpoint_id) final {
    disconnect_calls_.push_back({.client = client, .endpoint_id = endpoint_id});
  }

  // BaseBwuHandler:
  ByteArray HandleInitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& upgrade_service_id,
      const std::string& endpoint_id) final {
    handle_initialize_calls_.push_back({.client = client,
                                        .service_id = upgrade_service_id,
                                        .endpoint_id = endpoint_id});
    switch (medium_) {
      case location::nearby::proto::connections::BLUETOOTH:
        return parser::ForBwuBluetoothPathAvailable(
            upgrade_service_id,
            /*mac_address=*/"mac-address");
      case location::nearby::proto::connections::WIFI_LAN:
        return parser::ForBwuWifiLanPathAvailable(/*ip_address=*/"ABCD",
                                                  /*port=*/1234);
      case location::nearby::proto::connections::WEB_RTC:
        return parser::ForBwuWebrtcPathAvailable(
            /*peer_id=*/"peer-id",
            location::nearby::connections::LocationHint{});
      case location::nearby::proto::connections::WIFI_HOTSPOT:
        return parser::ForBwuWifiHotspotPathAvailable(
            /*ssid=*/"Direct-357a2d8c", /*password=*/"b592f7d3",
            /*port=*/1234, /*frequency=*/2412, /*gateway=*/"123.234.23.1",
            false);
      case location::nearby::proto::connections::WIFI_DIRECT:
        return parser::ForBwuWifiDirectPathAvailable(
            /*ssid=*/"Direct-12345678", /*password=*/"87654321", /*port=*/2143,
            /*frequency=*/2412, /*supports_disabling_encryption=*/false,
            /*gateway=*/"123.234.23.1");
      case location::nearby::proto::connections::UNKNOWN_MEDIUM:
      case location::nearby::proto::connections::MDNS:
      case location::nearby::proto::connections::BLE:
      case location::nearby::proto::connections::WIFI_AWARE:
      case location::nearby::proto::connections::NFC:
      case location::nearby::proto::connections::BLE_L2CAP:
      case location::nearby::proto::connections::USB:
        return ByteArray{};
    }
  }

  void HandleRevertInitiatorStateForService(
      const std::string& upgrade_service_id) final {
    handle_revert_calls_.push_back({.service_id = upgrade_service_id});
  }

  Medium medium_;
  std::vector<InputData> create_calls_;
  std::vector<InputData> disconnect_calls_;
  std::vector<InputData> handle_initialize_calls_;
  std::vector<InputData> handle_revert_calls_;
};

}  // namespace connections
}  // namespace nearby

#endif  // NEARBY_CONNECTIONS_IMPLEMENTATION_FAKE_BWU_HANDLER_H_
