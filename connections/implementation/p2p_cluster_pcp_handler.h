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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "connections/advertising_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/base_pcp_handler.h"
#include "connections/implementation/ble_advertisement.h"
#include "connections/implementation/bluetooth_device_name.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/injected_bluetooth_device_store.h"
#include "connections/implementation/mediums/awdl.h"
#include "connections/implementation/mediums/ble.h"
#include "connections/implementation/mediums/ble/ble_socket.h"
#include "connections/implementation/mediums/bluetooth_classic.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/mediums/wifi_direct.h"
#include "connections/implementation/mediums/wifi_hotspot.h"
#include "connections/implementation/mediums/wifi_lan.h"
#include "connections/implementation/webrtc_state.h"
#include "connections/medium_selector.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/power_level.h"
#include "connections/status.h"
#include "connections/v3/connection_listening_options.h"
#include "internal/interop/device.h"
#include "internal/platform/awdl.h"
#include "internal/platform/ble.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/wifi_lan.h"
#ifdef NO_WEBRTC
#include "connections/implementation/mediums/webrtc_socket_stub.h"
#include "connections/implementation/mediums/webrtc_stub.h"
#else
#include "connections/implementation/mediums/webrtc.h"
#endif
#include "connections/implementation/pcp.h"
#include "connections/implementation/wifi_lan_service_info.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/expected.h"

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
    BleEndpointState(const std::string& endpoint_id,
                     const ByteArray& endpoint_info)
        : endpoint_id(endpoint_id), endpoint_info(endpoint_info) {}

    std::string endpoint_id;
    ByteArray endpoint_info;
  };

  struct BleEndpointFeatureState {
    bool ble = false;
    bool l2cap = false;
    bool bt = false;
  };

  using BluetoothDiscoveredDeviceCallback =
      BluetoothClassic::DiscoveredDeviceCallback;
  using BleDiscoveredPeripheralCallback = Ble::DiscoveredPeripheralCallback;
  using WifiLanDiscoveredServiceCallback = WifiLan::DiscoveredServiceCallback;
  using AwdlDiscoveredServiceCallback = Awdl::DiscoveredServiceCallback;

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
  ErrorOr<location::nearby::proto::connections::Medium>
  StartBluetoothAdvertising(ClientProxy* client, const std::string& service_id,
                            const ByteArray& service_id_hash,
                            const std::string& local_endpoint_id,
                            const ByteArray& local_endpoint_info,
                            WebRtcState web_rtc_state);
  ErrorOr<location::nearby::proto::connections::Medium> StartBluetoothDiscovery(
      ClientProxy* client, const std::string& service_id);
  void StartBluetoothDiscoveryWithPause(
      ClientProxy* client, const std::string& service_id,
      const DiscoveryOptions& discovery_options,
      std::vector<Medium>& mediums_started_successfully,
      std::vector<location::nearby::analytics::proto::ConnectionsLog::
                      OperationResultWithMedium>& operation_result_with_mediums,
      int update_index);
  BasePcpHandler::ConnectImplResult BluetoothConnectImpl(
      ClientProxy* client, BluetoothEndpoint* endpoint);

  // Ble
  bool IsRecognizedBleEndpoint(absl::string_view service_id,
                               const BleAdvertisement& advertisement) const;
  void BlePeripheralDiscoveredHandler(ClientProxy* client,
                                      BlePeripheral peripheral,
                                      const std::string& service_id,
                                      const ByteArray& advertisement_bytes,
                                      bool fast_advertisement);
  void BlePeripheralLostHandler(ClientProxy* client, BlePeripheral peripheral,
                                const std::string& service_id,
                                const ByteArray& advertisement_bytes,
                                bool fast_advertisement);
  void BleInstantLostHandler(ClientProxy* client, BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement);
  void BleLegacyDeviceDiscoveredHandler();
  void BleConnectionAcceptedHandler(ClientProxy* client,
                                    absl::string_view local_endpoint_info,
                                    NearbyDevice::Type device_type,
                                    BleSocket socket,
                                    const std::string& service_id);
  void BleL2capConnectionAcceptedHandler(ClientProxy* client,
                                         absl::string_view local_endpoint_info,
                                         NearbyDevice::Type device_type,
                                         BleL2capSocket socket,
                                         const std::string& service_id);
  // The refactor version of BleConnectionAcceptedHandler() and
  // BleL2capConnectionAcceptedHandler above.
  void BleConnectionAcceptedHandler2(ClientProxy* client,
                                     absl::string_view local_endpoint_info,
                                     NearbyDevice::Type device_type,
                                     std::unique_ptr<mediums::BleSocket> socket,
                                     const std::string& service_id);
  ErrorOr<location::nearby::proto::connections::Medium> StartBleAdvertising(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info,
      const AdvertisingOptions& advertising_options, WebRtcState web_rtc_state);
  ErrorOr<location::nearby::proto::connections::Medium> StartBleScanning(
      ClientProxy* client, const std::string& service_id,
      const DiscoveryOptions& discovery_options);
  BasePcpHandler::ConnectImplResult BleConnectImpl(ClientProxy* client,
                                                   BleEndpoint* endpoint);
  // Awdl
  void AwdlServiceDiscoveredHandler(ClientProxy* client,
                                    NsdServiceInfo service_info,
                                    const std::string& service_id);
  void AwdlServiceLostHandler(ClientProxy* client, NsdServiceInfo service_info,
                              const std::string& service_id);
  void AwdlConnectionAcceptedHandler(ClientProxy* client,
                                     absl::string_view local_endpoint_id,
                                     absl::string_view local_endpoint_info,
                                     NearbyDevice::Type device_type,
                                     const std::string& service_id,
                                     AwdlSocket socket);
  BasePcpHandler::ConnectImplResult AwdlConnectImpl(ClientProxy* client,
                                                    AwdlEndpoint* endpoint);
  ErrorOr<location::nearby::proto::connections::Medium> StartAwdlAdvertising(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info, WebRtcState web_rtc_state);
  ErrorOr<location::nearby::proto::connections::Medium> StartAwdlDiscovery(
      ClientProxy* client, const std::string& service_id);

  // WifiLan
  bool IsRecognizedWifiServiceEndpoint(
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
  ErrorOr<location::nearby::proto::connections::Medium> StartWifiLanAdvertising(
      ClientProxy* client, const std::string& service_id,
      const std::string& local_endpoint_id,
      const ByteArray& local_endpoint_info, WebRtcState web_rtc_state);
  ErrorOr<location::nearby::proto::connections::Medium> StartWifiLanDiscovery(
      ClientProxy* client, const std::string& service_id);
  BasePcpHandler::ConnectImplResult WifiLanConnectImpl(
      ClientProxy* client, WifiLanEndpoint* endpoint);

  // Endpoints injection.
  Status InjectBluetoothEndpoint(ClientProxy* client,
                                 const std::string& service_id,
                                 const OutOfBandConnectionMetadata& metadata);
  Status InjectBleEndpoint(ClientProxy* client, const std::string& service_id,
                           const OutOfBandConnectionMetadata& metadata);

  Awdl& awdl_medium_;
  BluetoothRadio& bluetooth_radio_;
  BluetoothClassic& bluetooth_medium_;
  Ble& ble_medium_;
  WifiLan& wifi_lan_medium_;
  WifiHotspot& wifi_hotspot_medium_;
  WifiDirect& wifi_direct_medium_;
  mediums::WebRtc& webrtc_medium_;
  InjectedBluetoothDeviceStore& injected_bluetooth_device_store_;
  // Maintains a map of client_id to service_id for bluetooth classic
  // discoverer.
  absl::flat_hash_map<std::int64_t, std::string>
      bluetooth_classic_client_id_to_service_id_map_;
  std::int64_t bluetooth_classic_advertiser_client_id_{0};

  // Maps a BlePeripheral to its corresponding BleEndpointState.
  absl::flat_hash_map<std::string, BleEndpointState> found_ble_endpoints_;

  // Maps a BlePeripheral.Id_ to its corresponding BleEndpointState.
  absl::flat_hash_map<ByteArray, BleEndpointFeatureState>
      found_endpoints_in_ble_discover_cb_;

  // Maps service id to its client.
  absl::flat_hash_map<std::string, ClientProxy*>
      paused_bluetooth_clients_discoveries_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_P2P_CLUSTER_PCP_HANDLER_H_
