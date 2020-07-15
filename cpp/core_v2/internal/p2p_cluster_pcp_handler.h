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

#ifndef CORE_V2_INTERNAL_P2P_CLUSTER_PCP_HANDLER_H_
#define CORE_V2_INTERNAL_P2P_CLUSTER_PCP_HANDLER_H_

#include <memory>
#include <vector>

#include "core_v2/internal/base_pcp_handler.h"
#include "core_v2/internal/ble_advertisement.h"
#include "core_v2/internal/bluetooth_device_name.h"
#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/endpoint_channel_manager.h"
#include "core_v2/internal/endpoint_manager.h"
#include "core_v2/internal/mediums/bluetooth_classic.h"
#include "core_v2/internal/mediums/mediums.h"
#include "core_v2/internal/mediums/webrtc.h"
#include "core_v2/internal/mediums/webrtc/peer_id.h"
#include "core_v2/internal/pcp.h"
#include "core_v2/internal/wifi_lan_service_info.h"
#include "core_v2/options.h"
#include "core_v2/strategy.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/public/bluetooth_classic.h"
#include "platform_v2/public/wifi_lan.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

// Concrete implementation of the PCPHandler for the P2P_CLUSTER PCP. This PCP
// is reserved for mediums that can connect to multiple devices simultaneously
// and all devices are considered equal. For asymmetric mediums, where one
// device is a server and the others are clients, use P2PStarPCPHandler instead.
//
// Currently, this implementation advertises/discovers over Bluetooth and
// connects over Bluetooth.
class P2pClusterPcpHandler : public BasePcpHandler {
 public:
  P2pClusterPcpHandler(Mediums& mediums, EndpointManager* endpoint_manager,
                       EndpointChannelManager* channel_manager,
                       Pcp pcp = Pcp::kP2pCluster);
  ~P2pClusterPcpHandler() override = default;

 protected:
  std::vector<proto::connections::Medium> GetConnectionMediumsByPriority()
      override;
  proto::connections::Medium GetDefaultUpgradeMedium() override;

  // @PCPHandlerThread
  BasePcpHandler::StartOperationResult StartAdvertisingImpl(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const std::string& local_endpoint_name,
      const ConnectionOptions& options) override;

  // @PCPHandlerThread
  Status StopAdvertisingImpl(ClientProxy* client) override;

  // @PCPHandlerThread
  BasePcpHandler::StartOperationResult StartDiscoveryImpl(
      ClientProxy* client, const std::string& service_id,
      const ConnectionOptions& options) override;

  // @PCPHandlerThread
  Status StopDiscoveryImpl(ClientProxy* client) override;

  // @PCPHandlerThread
  BasePcpHandler::ConnectImplResult ConnectImpl(
      ClientProxy* client,
      BasePcpHandler::DiscoveredEndpoint* endpoint) override;

 private:
  struct BluetoothEndpoint : public BasePcpHandler::DiscoveredEndpoint {
    BluetoothEndpoint(DiscoveredEndpoint endpoint, BluetoothDevice device)
        : DiscoveredEndpoint(std::move(endpoint)),
          bluetooth_device(std::move(device)) {}

    BluetoothDevice bluetooth_device;
  };
  struct WifiLanEndpoint : public BasePcpHandler::DiscoveredEndpoint {
    WifiLanEndpoint(DiscoveredEndpoint endpoint, WifiLanService service)
        : DiscoveredEndpoint(std::move(endpoint)),
          wifi_lan_service(std::move(service)) {}
    WifiLanService wifi_lan_service;
  };

  using BluetoothDiscoveredDeviceCallback =
      BluetoothClassic::DiscoveredDeviceCallback;
  using WifiLanDiscoveredServiceCallback = WifiLan::DiscoveredServiceCallback;

  static constexpr BluetoothDeviceName::Version kBluetoothDeviceNameVersion =
      BluetoothDeviceName::Version::kV1;
  static constexpr WifiLanServiceInfo::Version kWifiLanServiceInfoVersion =
      WifiLanServiceInfo::Version::kV1;

  static ByteArray GenerateHash(const std::string& source, size_t size);

  // Bluetooth
  bool IsRecognizedBluetoothEndpoint(const std::string& name_string,
                                     const std::string& service_id,
                                     const BluetoothDeviceName& name) const;
  std::function<void(BluetoothDevice&)> MakeBluetoothDeviceDiscoveredHandler(
      ClientProxy* client, const std::string& service_id);
  std::function<void(BluetoothDevice&)> MakeBluetoothDeviceLostHandler(
      ClientProxy* client, const std::string& service_id);
  proto::connections::Medium StartBluetoothAdvertising(
      ClientProxy* client, const std::string& service_id,
      const ByteArray& service_id_hash, const std::string& local_endpoint_id,
      const std::string& local_endpoint_name);
  proto::connections::Medium StartBluetoothDiscovery(
      BluetoothDiscoveredDeviceCallback callback, ClientProxy* client,
      const std::string& service_id);
  BasePcpHandler::ConnectImplResult BluetoothConnectImpl(
      ClientProxy* client, BluetoothEndpoint* endpoint);

  // WifiLan
  bool IsRecognizedWifiLanEndpoint(
      const std::string& service_id,
      const WifiLanServiceInfo& service_info) const;
  std::function<void(WifiLanService&, const std::string&)>
  MakeWifiLanServiceDiscoveredHandler(ClientProxy* client,
                                      const std::string& service_id);
  std::function<void(WifiLanService&, const std::string&)>
  MakeWifiLanServiceLostHandler(ClientProxy* client,
                                const std::string& service_id);
  proto::connections::Medium StartWifiLanAdvertising(
      ClientProxy* client, const std::string& service_id,
      const ByteArray& service_id_hash, const std::string& local_endpoint_id,
      const std::string& local_endpoint_name);
  proto::connections::Medium StartWifiLanDiscovery(
      WifiLanDiscoveredServiceCallback callback, ClientProxy* client,
      const std::string& service_id);
  BasePcpHandler::ConnectImplResult WifiLanConnectImpl(
      ClientProxy* client, WifiLanEndpoint* endpoint);

  // WebRtc
  proto::connections::Medium StartListeningForWebRtcConnections(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const std::string& local_endpoint_name);
  BasePcpHandler::ConnectImplResult WebRtcConnectImpl(
      ClientProxy* client, WebRtcEndpoint* webrtc_endpoint);

  BluetoothRadio& bluetooth_radio_;
  BluetoothClassic& bluetooth_medium_;
  WifiLan& wifi_lan_medium_;
  mediums::WebRtc& webrtc_medium_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_P2P_CLUSTER_PCP_HANDLER_H_
