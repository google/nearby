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
#include "core_v2/internal/bwu_manager.h"
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
  P2pClusterPcpHandler(Mediums* mediums, EndpointManager* endpoint_manager,
                       EndpointChannelManager* channel_manager,
                       BwuManager* bwu_manager,
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
      const ByteArray& local_endpoint_info,
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
  struct BleEndpoint : public BasePcpHandler::DiscoveredEndpoint {
    BleEndpoint(DiscoveredEndpoint endpoint, BlePeripheral peripheral)
        : DiscoveredEndpoint(std::move(endpoint)),
          ble_peripheral(std::move(peripheral)) {}
    BlePeripheral ble_peripheral;
  };

  // Holds the state required to re-create a BleEndpoint we see on a
  // BlePeripheral, so BlePeripheralLostHandler can call
  // BasePcpHandler::OnEndpointLost() with the same information as was passed
  // in to BasePCPHandler::onEndpointFound().
  struct BleEndpointState {
   public:
    BleEndpointState(const string& endpoint_id, const ByteArray& endpoint_info)
        : endpoint_id(endpoint_id), endpoint_info(endpoint_info) {}

    std::string endpoint_id;
    ByteArray endpoint_info;
  };
  struct WifiLanEndpoint : public BasePcpHandler::DiscoveredEndpoint {
    WifiLanEndpoint(DiscoveredEndpoint endpoint, WifiLanService service)
        : DiscoveredEndpoint(std::move(endpoint)),
          wifi_lan_service(std::move(service)) {}

    WifiLanService wifi_lan_service;
  };

  using BluetoothDiscoveredDeviceCallback =
      BluetoothClassic::DiscoveredDeviceCallback;
  using BleDiscoveredPeripheralCallback = Ble::DiscoveredPeripheralCallback;
  using WifiLanDiscoveredServiceCallback = WifiLan::DiscoveredServiceCallback;

  static constexpr BluetoothDeviceName::Version kBluetoothDeviceNameVersion =
      BluetoothDeviceName::Version::kV1;
  static constexpr BleAdvertisement::Version kBleAdvertisementVersion =
      BleAdvertisement::Version::kV1;
  static constexpr WifiLanServiceInfo::Version kWifiLanServiceInfoVersion =
      WifiLanServiceInfo::Version::kV1;

  static ByteArray GenerateHash(const std::string& source, size_t size);
  static bool ShouldAdvertiseBluetoothMacOverBle(PowerLevel power_level);
  static bool ShouldAcceptBluetoothConnections(
      const ConnectionOptions& options);

  // Bluetooth
  bool IsRecognizedBluetoothEndpoint(const std::string& name_string,
                                     const std::string& service_id,
                                     const BluetoothDeviceName& name) const;
  void BluetoothDeviceDiscoveredHandler(ClientProxy* client,
                                        const std::string& service_id,
                                        BluetoothDevice& device);
  void BluetoothDeviceLostHandler(ClientProxy* client,
                                  const std::string& service_id,
                                  BluetoothDevice& device);
  proto::connections::Medium StartBluetoothAdvertising(
      ClientProxy* client, const std::string& service_id,
      const ByteArray& service_id_hash, const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info);
  proto::connections::Medium StartBluetoothDiscovery(
      BluetoothDiscoveredDeviceCallback callback, ClientProxy* client,
      const std::string& service_id);
  BasePcpHandler::ConnectImplResult BluetoothConnectImpl(
      ClientProxy* client, BluetoothEndpoint* endpoint);

  // Ble
  // Maps a BlePeripheral to its corresponding BleEndpointState.
  absl::flat_hash_map<std::string, BleEndpointState> found_ble_endpoints_;
  bool IsRecognizedBleEndpoint(const std::string& service_id,
                               const BleAdvertisement& advertisement) const;
  void BlePeripheralDiscoveredHandler(ClientProxy* client,
                                      BlePeripheral& peripheral,
                                      const std::string& service_id,
                                      bool fast_advertisement);
  void BlePeripheralLostHandler(ClientProxy* client, BlePeripheral& peripheral,
                                const std::string& service_id);
  proto::connections::Medium StartBleAdvertising(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info, const ConnectionOptions& options);
  proto::connections::Medium StartBleScanning(
      BleDiscoveredPeripheralCallback callback, ClientProxy* client,
      const std::string& service_id,
      const std::string& fast_advertisement_service_uuid);
  BasePcpHandler::ConnectImplResult BleConnectImpl(ClientProxy* client,
                                                   BleEndpoint* endpoint);

  // WifiLan
  bool IsRecognizedWifiLanEndpoint(
      const std::string& service_id,
      const WifiLanServiceInfo& service_info) const;
  void WifiLanServiceDiscoveredHandler(ClientProxy* client,
                                       WifiLanService& service,
                                       const std::string& service_id);
  void WifiLanServiceLostHandler(ClientProxy* client, WifiLanService& service,
                                 const std::string& service_id);
  proto::connections::Medium StartWifiLanAdvertising(
      ClientProxy* client, const std::string& service_id,
      const ByteArray& service_id_hash, const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info);
  proto::connections::Medium StartWifiLanDiscovery(
      WifiLanDiscoveredServiceCallback callback, ClientProxy* client,
      const std::string& service_id);
  BasePcpHandler::ConnectImplResult WifiLanConnectImpl(
      ClientProxy* client, WifiLanEndpoint* endpoint);

  // WebRtc
  proto::connections::Medium StartListeningForWebRtcConnections(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info);
  BasePcpHandler::ConnectImplResult WebRtcConnectImpl(
      ClientProxy* client, WebRtcEndpoint* webrtc_endpoint);

  BluetoothRadio& bluetooth_radio_;
  BluetoothClassic& bluetooth_medium_;
  Ble& ble_medium_;
  WifiLan& wifi_lan_medium_;
  mediums::WebRtc& webrtc_medium_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_P2P_CLUSTER_PCP_HANDLER_H_
