// Copyright 2021 Google LLC
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

#ifndef CORE_INTERNAL_P2P_CLUSTER_PCP_HANDLER_H_
#define CORE_INTERNAL_P2P_CLUSTER_PCP_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "connections/implementation/base_pcp_handler.h"
#include "connections/implementation/ble_advertisement.h"
#include "connections/implementation/bluetooth_device_name.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/injected_bluetooth_device_store.h"
#include "connections/implementation/mediums/bluetooth_classic.h"
#include "connections/implementation/mediums/mediums.h"
#ifdef NO_WEBRTC
#include "connections/implementation/mediums/webrtc_socket_stub.h"
#include "connections/implementation/mediums/webrtc_stub.h"
#else
#include "connections/implementation/mediums/webrtc.h"
#endif
#include "connections/implementation/pcp.h"
#include "connections/implementation/wifi_lan_service_info.h"
#include "internal/platform/byte_array.h"

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
  P2pClusterPcpHandler(
      Mediums* mediums, EndpointManager* endpoint_manager,
      EndpointChannelManager* channel_manager, BwuManager* bwu_manager,
      InjectedBluetoothDeviceStore& injected_bluetooth_device_store,
      Pcp pcp = Pcp::kP2pCluster);
  ~P2pClusterPcpHandler() override;

 protected:
  std::vector<location::nearby::proto::connections::Medium>
  GetConnectionMediumsByPriority() override;
  location::nearby::proto::connections::Medium GetDefaultUpgradeMedium()
      override;

  // @PCPHandlerThread
  BasePcpHandler::StartOperationResult StartAdvertisingImpl(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info,
      const AdvertisingOptions& advertising_options) override;

  // @PCPHandlerThread
  Status StopAdvertisingImpl(ClientProxy* client) override;

  // @PCPHandlerThread
  BasePcpHandler::StartOperationResult StartDiscoveryImpl(
      ClientProxy* client, const std::string& service_id,
      const DiscoveryOptions& discovery_options) override;

  // @PCPHandlerThread
  Status StopDiscoveryImpl(ClientProxy* client) override;

  // @PCPHandlerThread
  Status InjectEndpointImpl(
      ClientProxy* client, const std::string& service_id,
      const OutOfBandConnectionMetadata& metadata) override;

  // @PCPHandlerThread
  BasePcpHandler::ConnectImplResult ConnectImpl(
      ClientProxy* client,
      BasePcpHandler::DiscoveredEndpoint* endpoint) override;

  // @PCPHandlerThread
  BasePcpHandler::StartOperationResult StartListeningForIncomingConnectionsImpl(
      ClientProxy* client_proxy, absl::string_view service_id,
      absl::string_view local_endpoint_id,
      v3::ConnectionListeningOptions options) override;

  // @PCPHandlerThread
  void StopListeningForIncomingConnectionsImpl(ClientProxy* client) override;

  // @PCPHandlerThread
  BasePcpHandler::StartOperationResult UpdateAdvertisingOptionsImpl(
      ClientProxy* client, absl::string_view service_id,
      absl::string_view local_endpoint_id,
      absl::string_view local_endpoint_info,
      const AdvertisingOptions& advertising_options) override;

  // @PCPHandlerThread
  BasePcpHandler::StartOperationResult UpdateDiscoveryOptionsImpl(
      ClientProxy* client, absl::string_view service_id,
      absl::string_view local_endpoint_id,
      absl::string_view local_endpoint_info,
      const DiscoveryOptions& discovery_options)
      RUN_ON_PCP_HANDLER_THREAD() override;

 private:
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

  struct BleV2EndpointState {
    bool ble = false;
    bool l2cap = false;
    bool bt = false;
  };

  using BluetoothDiscoveredDeviceCallback =
      BluetoothClassic::DiscoveredDeviceCallback;
  using BleDiscoveredPeripheralCallback = Ble::DiscoveredPeripheralCallback;
  using BleV2DiscoveredPeripheralCallback = BleV2::DiscoveredPeripheralCallback;
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
      const AdvertisingOptions& advertising_options);

  // Bluetooth
  bool IsRecognizedBluetoothEndpoint(const std::string& name_string,
                                     const std::string& service_id,
                                     const BluetoothDeviceName& name) const;
  void BluetoothDeviceDiscoveredHandler(ClientProxy* client,
                                        const std::string& service_id,
                                        BluetoothDevice device);
  void BluetoothNameChangedHandler(ClientProxy* client,
                                   const std::string& service_id,
                                   BluetoothDevice device);
  void BluetoothDeviceLostHandler(ClientProxy* client,
                                  const std::string& service_id,
                                  BluetoothDevice& device);
  void BluetoothConnectionAcceptedHandler(ClientProxy* client,
                                          absl::string_view local_endpoint_info,
                                          NearbyDevice::Type device_type,
                                          const std::string& service_id,
                                          BluetoothSocket socket);
  location::nearby::proto::connections::Medium StartBluetoothAdvertising(
      ClientProxy* client, const std::string& service_id,
      const ByteArray& service_id_hash, const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info, WebRtcState web_rtc_state);
  location::nearby::proto::connections::Medium StartBluetoothDiscovery(
      ClientProxy* client, const std::string& service_id);
  BasePcpHandler::ConnectImplResult BluetoothConnectImpl(
      ClientProxy* client, BluetoothEndpoint* endpoint);

  // Ble
  bool IsRecognizedBleEndpoint(const std::string& service_id,
                               const BleAdvertisement& advertisement) const;
  void BlePeripheralDiscoveredHandler(ClientProxy* client,
                                      BlePeripheral& peripheral,
                                      const std::string& service_id,
                                      const ByteArray& advertisement_bytes,
                                      bool fast_advertisement);
  void BlePeripheralLostHandler(ClientProxy* client, BlePeripheral& peripheral,
                                const std::string& service_id);
  void BleConnectionAcceptedHandler(ClientProxy* client,
                                    absl::string_view local_endpoint_info,
                                    NearbyDevice::Type device_type,
                                    BleSocket socket,
                                    const std::string& service_id);
  location::nearby::proto::connections::Medium StartBleAdvertising(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info,
      const AdvertisingOptions& advertising_options, WebRtcState web_rtc_state);
  location::nearby::proto::connections::Medium StartBleScanning(
      ClientProxy* client, const std::string& service_id,
      const std::string& fast_advertisement_service_uuid);
  BasePcpHandler::ConnectImplResult BleConnectImpl(ClientProxy* client,
                                                   BleEndpoint* endpoint);

  // BleV2
  bool IsRecognizedBleV2Endpoint(absl::string_view service_id,
                                 const BleAdvertisement& advertisement) const;
  void BleV2PeripheralDiscoveredHandler(ClientProxy* client,
                                        BleV2Peripheral peripheral,
                                        const std::string& service_id,
                                        const ByteArray& advertisement_bytes,
                                        bool fast_advertisement);
  void BleV2PeripheralLostHandler(ClientProxy* client,
                                  BleV2Peripheral peripheral,
                                  const std::string& service_id,
                                  const ByteArray& advertisement_bytes,
                                  bool fast_advertisement);
  void BleV2ConnectionAcceptedHandler(ClientProxy* client,
                                      absl::string_view local_endpoint_info,
                                      NearbyDevice::Type device_type,
                                      BleV2Socket socket,
                                      const std::string& service_id);
  location::nearby::proto::connections::Medium StartBleV2Advertising(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info,
      const AdvertisingOptions& advertising_options, WebRtcState web_rtc_state);
  location::nearby::proto::connections::Medium StartBleV2Scanning(
      ClientProxy* client, const std::string& service_id,
      const DiscoveryOptions& discovery_options);
  BasePcpHandler::ConnectImplResult BleV2ConnectImpl(ClientProxy* client,
                                                     BleV2Endpoint* endpoint);

  // WifiLan
  bool IsRecognizedWifiLanEndpoint(
      const std::string& service_id,
      const WifiLanServiceInfo& wifi_lan_service_info) const;
  void WifiLanServiceDiscoveredHandler(ClientProxy* client,
                                       NsdServiceInfo service_info,
                                       const std::string& service_id);
  void WifiLanServiceLostHandler(ClientProxy* client,
                                 NsdServiceInfo service_info,
                                 const std::string& service_id);
  void WifiLanConnectionAcceptedHandler(ClientProxy* client,
                                        absl::string_view local_endpoint_id,
                                        absl::string_view local_endpoint_info,
                                        NearbyDevice::Type device_type,
                                        const std::string& service_id,
                                        WifiLanSocket socket);
  location::nearby::proto::connections::Medium StartWifiLanAdvertising(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info, WebRtcState web_rtc_state);
  location::nearby::proto::connections::Medium StartWifiLanDiscovery(
      ClientProxy* client, const std::string& service_id);
  BasePcpHandler::ConnectImplResult WifiLanConnectImpl(
      ClientProxy* client, WifiLanEndpoint* endpoint);

  BluetoothRadio& bluetooth_radio_;
  BluetoothClassic& bluetooth_medium_;
  Ble& ble_medium_;
  BleV2& ble_v2_medium_;
  WifiLan& wifi_lan_medium_;
  WifiHotspot& wifi_hotspot_medium_;
  WifiDirect& wifi_direct_medium_;
  mediums::WebRtc& webrtc_medium_;
  InjectedBluetoothDeviceStore& injected_bluetooth_device_store_;
  std::int64_t bluetooth_classic_discoverer_client_id_{0};
  std::int64_t bluetooth_classic_advertiser_client_id_{0};

  // Maps a BlePeripheral to its corresponding BleEndpointState.
  absl::flat_hash_map<std::string, BleEndpointState> found_ble_endpoints_;

  // Maps a BlePeripheral.Id_ to its corresponding BleEndpointState.
  absl::flat_hash_map<ByteArray, BleV2EndpointState>
      found_endpoints_in_ble_discover_cb_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_P2P_CLUSTER_PCP_HANDLER_H_
