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

#include "connections/implementation/p2p_cluster_pcp_handler.h"

#include "absl/functional/bind_front.h"
#include "absl/strings/escaping.h"
#include "connections/implementation/base_pcp_handler.h"
#include "connections/implementation/ble_advertisement.h"
#include "connections/implementation/ble_endpoint_channel.h"
#include "connections/implementation/bluetooth_endpoint_channel.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/webrtc_endpoint_channel.h"
#include "connections/implementation/wifi_lan_endpoint_channel.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/types.h"
#include "internal/platform/crypto.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {

ByteArray P2pClusterPcpHandler::GenerateHash(const std::string& source,
                                             size_t size) {
  return Utils::Sha256Hash(source, size);
}

bool P2pClusterPcpHandler::ShouldAdvertiseBluetoothMacOverBle(
    PowerLevel power_level) {
  return power_level == PowerLevel::kHighPower;
}

bool P2pClusterPcpHandler::ShouldAcceptBluetoothConnections(
    const AdvertisingOptions& advertising_options) {
  return advertising_options.enable_bluetooth_listening;
}

P2pClusterPcpHandler::P2pClusterPcpHandler(
    Mediums* mediums, EndpointManager* endpoint_manager,
    EndpointChannelManager* endpoint_channel_manager, BwuManager* bwu_manager,
    InjectedBluetoothDeviceStore& injected_bluetooth_device_store, Pcp pcp)
    : BasePcpHandler(mediums, endpoint_manager, endpoint_channel_manager,
                     bwu_manager, pcp),
      bluetooth_radio_(mediums->GetBluetoothRadio()),
      bluetooth_medium_(mediums->GetBluetoothClassic()),
      ble_medium_(mediums->GetBle()),
      wifi_lan_medium_(mediums->GetWifiLan()),
      webrtc_medium_(mediums->GetWebRtc()),
      injected_bluetooth_device_store_(injected_bluetooth_device_store) {}

// Returns a vector or mediums sorted in order or decreasing priority for
// all the supported mediums.
// Example: WiFi_LAN, WEB_RTC, BT, BLE
std::vector<proto::connections::Medium>
P2pClusterPcpHandler::GetConnectionMediumsByPriority() {
  std::vector<proto::connections::Medium> mediums;
  if (wifi_lan_medium_.IsAvailable()) {
    mediums.push_back(proto::connections::WIFI_LAN);
  }
  if (webrtc_medium_.IsAvailable()) {
    mediums.push_back(proto::connections::WEB_RTC);
  }
  if (bluetooth_medium_.IsAvailable()) {
    mediums.push_back(proto::connections::BLUETOOTH);
  }
  if (ble_medium_.IsAvailable()) {
    mediums.push_back(proto::connections::BLE);
  }
  return mediums;
}

proto::connections::Medium P2pClusterPcpHandler::GetDefaultUpgradeMedium() {
  return proto::connections::WIFI_LAN;
}

BasePcpHandler::StartOperationResult P2pClusterPcpHandler::StartAdvertisingImpl(
    ClientProxy* client, const std::string& service_id,
    const std::string& local_endpoint_id, const ByteArray& local_endpoint_info,
    const AdvertisingOptions& advertising_options) {
  std::vector<proto::connections::Medium> mediums_started_successfully;

  WebRtcState web_rtc_state{WebRtcState::kUnconnectable};

  if (advertising_options.allowed.wifi_lan) {
    proto::connections::Medium wifi_lan_medium =
        StartWifiLanAdvertising(client, service_id, local_endpoint_id,
                                local_endpoint_info, web_rtc_state);
    if (wifi_lan_medium != proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOGS(INFO)
          << "P2pClusterPcpHandler::StartAdvertisingImpl: WifiLan added";
      mediums_started_successfully.push_back(wifi_lan_medium);
    }
  }

  if (advertising_options.allowed.bluetooth) {
    const ByteArray bluetooth_hash =
        GenerateHash(service_id, BluetoothDeviceName::kServiceIdHashLength);
    proto::connections::Medium bluetooth_medium = StartBluetoothAdvertising(
        client, service_id, bluetooth_hash, local_endpoint_id,
        local_endpoint_info, web_rtc_state);
    if (bluetooth_medium != proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOG(INFO, "P2pClusterPcpHandler::StartAdvertisingImpl: BT added");
      mediums_started_successfully.push_back(bluetooth_medium);
      bluetooth_classic_advertiser_client_id_ = client->GetClientId();
    }
  }

  if (advertising_options.allowed.ble) {
    proto::connections::Medium ble_medium = StartBleAdvertising(
        client, service_id, local_endpoint_id, local_endpoint_info,
        advertising_options, web_rtc_state);
    if (ble_medium != proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOGS(INFO)
          << "P2pClusterPcpHandler::StartAdvertisingImpl: Ble added";
      mediums_started_successfully.push_back(ble_medium);
    }
  }

  if (mediums_started_successfully.empty()) {
    NEARBY_LOGS(ERROR) << "Failed StartAdvertising("
                       << absl::BytesToHexString(local_endpoint_info.data())
                       << ") for client=" << client->GetClientId();
    return {
        .status = {Status::kBluetoothError},
    };
  }

  // The rest of the operations for startAdvertising() will continue
  // asynchronously via
  // IncomingBluetoothConnectionProcessor.onIncomingBluetoothConnection(), so
  // leave it to that to signal any errors that may occur.
  return {
      .status = {Status::kSuccess},
      .mediums = std::move(mediums_started_successfully),
  };
}

Status P2pClusterPcpHandler::StopAdvertisingImpl(ClientProxy* client) {
  if (client->GetClientId() == bluetooth_classic_advertiser_client_id_) {
    bluetooth_medium_.TurnOffDiscoverability();
    bluetooth_classic_advertiser_client_id_ = 0;
  } else {
    NEARBY_LOGS(INFO) << "Skipped BT TurnOffDiscoverability for client="
                      << client->GetClientId()
                      << ", client that turned on discoverability is "
                      << bluetooth_classic_advertiser_client_id_;
  }

  bluetooth_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());

  ble_medium_.StopAdvertising(client->GetAdvertisingServiceId());
  ble_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());

  wifi_lan_medium_.StopAdvertising(client->GetAdvertisingServiceId());
  wifi_lan_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());

  return {Status::kSuccess};
}

bool P2pClusterPcpHandler::IsRecognizedBluetoothEndpoint(
    const std::string& name_string, const std::string& service_id,
    const BluetoothDeviceName& name) const {
  if (!name.IsValid()) {
    NEARBY_LOGS(INFO)
        << name_string
        << " doesn't conform to the BluetoothDeviceName format, discarding.";
    return false;
  }

  if (name.GetPcp() != GetPcp()) {
    NEARBY_LOGS(INFO) << name_string << " doesn't match on Pcp; expected "
                      << PcpToStrategy(GetPcp()).GetName() << ", found "
                      << PcpToStrategy(name.GetPcp()).GetName();
    return false;
  }

  ByteArray expected_service_id_hash =
      GenerateHash(service_id, BluetoothDeviceName::kServiceIdHashLength);

  if (name.GetServiceIdHash() != expected_service_id_hash) {
    NEARBY_LOGS(INFO) << name_string
                      << " doesn't match on expected service_id_hash; expected "
                      << absl::BytesToHexString(expected_service_id_hash.data())
                      << ", found "
                      << absl::BytesToHexString(name.GetServiceIdHash().data());
    return false;
  }

  return true;
}

void P2pClusterPcpHandler::BluetoothDeviceDiscoveredHandler(
    ClientProxy* client, const std::string& service_id,
    BluetoothDevice device) {
  RunOnPcpHandlerThread(
      "p2p-bt-device-discovered",
      [this, client, service_id, device]()
          RUN_ON_PCP_HANDLER_THREAD() {
            // Make sure we are still discovering before proceeding.
            if (!client->IsDiscovering()) {
              NEARBY_LOGS(WARNING) << "Skipping discovery of BluetoothDevice "
                                   << device.GetName()
                                   << " because we are no longer discovering.";
              return;
            }

            // Parse the Bluetooth device name.
            const std::string device_name_string = device.GetName();
            BluetoothDeviceName device_name(device_name_string);

            // Make sure the Bluetooth device name points to a valid
            // endpoint we're discovering.
            if (!IsRecognizedBluetoothEndpoint(device_name_string, service_id,
                                               device_name)) {
              NEARBY_LOGS(INFO) << "Found unrecognized BluetoothDeviceName "
                                << device_name_string;
              return;
            }

            // Report the discovered endpoint to the client.
            NEARBY_LOGS(INFO)
                << "Found BluetoothDeviceName " << device_name_string
                << " (with endpoint_id=" << device_name.GetEndpointId()
                << " and endpoint_info="
                << absl::BytesToHexString(device_name.GetEndpointInfo().data())
                << ").";
            OnEndpointFound(
                client,
                std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
                    {device_name.GetEndpointId(), device_name.GetEndpointInfo(),
                     service_id, proto::connections::Medium::BLUETOOTH,
                     device_name.GetWebRtcState()},
                    device,
                }));
          });
}

void P2pClusterPcpHandler::BluetoothNameChangedHandler(
    ClientProxy* client, const std::string& service_id,
    BluetoothDevice device) {
  RunOnPcpHandlerThread(
      "p2p-bt-name-changed",
      [this, client, service_id, device]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering()) {
          NEARBY_LOGS(WARNING)
              << "Ignoring lost BluetoothDevice " << device.GetName()
              << " because Connections is no longer discovering.";
          return;
        }

        // Parse the Bluetooth device name.
        const std::string device_name_string = device.GetName();
        BluetoothDeviceName device_name(device_name_string);
        NEARBY_LOGS(INFO) << "BT discovery handler (CHANGED) [client_id="
                          << client->GetClientId()
                          << ", service_id=" << service_id
                          << "]: processing new name " << device_name_string;

        // By this point, the BluetoothDevice passed to us has a different name
        // than what we may have discovered before. We need to iterate over the
        // found BluetoothEndpoints and compare their addresses to see the
        // devices are the same. We are not guaranteed to discover a match,
        // since the old name may not have been formatted for Nearby
        // Connections.
        for (auto endpoint :
             GetDiscoveredEndpoints(proto::connections::Medium::BLUETOOTH)) {
          BluetoothEndpoint* bluetoothEndpoint =
              static_cast<BluetoothEndpoint*>(endpoint);
          NEARBY_LOGS(INFO)
              << "BT discovery handler (CHANGED) [client_id="
              << client->GetClientId() << ", service_id=" << service_id
              << "]: comparing MAC addresses with existing endpoint "
              << bluetoothEndpoint->bluetooth_device.GetName()
              << ". They have MAC address "
              << bluetoothEndpoint->bluetooth_device.GetMacAddress()
              << " and the new endpoint has MAC address "
              << device.GetMacAddress();
          if (bluetoothEndpoint->bluetooth_device.GetMacAddress() ==
              device.GetMacAddress()) {
            // Report the BluetoothEndpoint as lost to the client.
            NEARBY_LOGS(INFO) << "Reporting lost BluetoothDevice "
                              << bluetoothEndpoint->bluetooth_device.GetName()
                              << ", due to device name change.";
            OnEndpointLost(client, *endpoint);
            break;
          }
        }

        // Make sure the Bluetooth device name points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedBluetoothEndpoint(device_name_string, service_id,
                                           device_name)) {
          NEARBY_LOGS(INFO) << "Found unrecognized BluetoothDeviceName "
                            << device_name_string;
          return;
        }

        // Report the discovered endpoint to the client.
        NEARBY_LOGS(INFO) << "Found BluetoothDeviceName " << device_name_string
                          << " (with endpoint_id="
                          << device_name.GetEndpointId()
                          << " and endpoint_info="
                          << absl::BytesToHexString(
                                 device_name.GetEndpointInfo().data())
                          << ").";
        OnEndpointFound(
            client,
            std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
                {device_name.GetEndpointId(), device_name.GetEndpointInfo(),
                 service_id, proto::connections::Medium::BLUETOOTH,
                 device_name.GetWebRtcState()},
                device,
            }));
      });
}

void P2pClusterPcpHandler::BluetoothDeviceLostHandler(
    ClientProxy* client, const std::string& service_id,
    BluetoothDevice& device) {
  const std::string& device_name_string = device.GetName();
  RunOnPcpHandlerThread(
      "p2p-bt-device-lost", [this, client, service_id,
                             device_name_string]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering()) {
          NEARBY_LOGS(WARNING)
              << "Ignoring lost BluetoothDevice " << device_name_string
              << " because Connections is no "
                 "longer discovering.";
          return;
        }

        // Parse the Bluetooth device name.
        BluetoothDeviceName device_name(device_name_string);

        // Make sure the Bluetooth device name points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedBluetoothEndpoint(device_name_string, service_id,
                                           device_name))
          return;

        // Report the BluetoothEndpoint as lost to the client.
        NEARBY_LOGS(INFO) << "Processing lost BluetoothDeviceName "
                          << device_name_string;
        OnEndpointLost(client, DiscoveredEndpoint{
                                   device_name.GetEndpointId(),
                                   device_name.GetEndpointInfo(), service_id,
                                   proto::connections::Medium::BLUETOOTH,
                                   WebRtcState::kUndefined});
      });
}

bool P2pClusterPcpHandler::IsRecognizedBleEndpoint(
    const std::string& service_id,
    const BleAdvertisement& advertisement) const {
  if (!advertisement.IsValid()) {
    NEARBY_LOGS(INFO)
        << "BleAdvertisement doesn't conform to the format, discarding.";
    return false;
  }

  if (advertisement.GetVersion() != kBleAdvertisementVersion) {
    NEARBY_LOGS(INFO) << "BleAdvertisement has an unknown version; expected "
                      << static_cast<int>(kBleAdvertisementVersion)
                      << ", found "
                      << static_cast<int>(advertisement.GetVersion());
    return false;
  }

  if (advertisement.GetPcp() != GetPcp()) {
    NEARBY_LOGS(INFO) << "BleAdvertisement doesn't match on Pcp; expected "
                      << PcpToStrategy(GetPcp()).GetName() << ", found "
                      << PcpToStrategy(advertisement.GetPcp()).GetName();
    return false;
  }

  // Check ServiceId for normal advertisement.
  // ServiceIdHash is empty for fast advertisement.
  if (!advertisement.IsFastAdvertisement()) {
    ByteArray expected_service_id_hash =
        GenerateHash(service_id, BleAdvertisement::kServiceIdHashLength);

    if (advertisement.GetServiceIdHash() != expected_service_id_hash) {
      NEARBY_LOGS(INFO)
          << "BleAdvertisement doesn't match on expected service_id_hash; "
             "expected "
          << absl::BytesToHexString(expected_service_id_hash.data())
          << ", found "
          << absl::BytesToHexString(advertisement.GetServiceIdHash().data());
      return false;
    }
  }

  return true;
}

void P2pClusterPcpHandler::BlePeripheralDiscoveredHandler(
    ClientProxy* client, BlePeripheral& peripheral,
    const std::string& service_id, const ByteArray& advertisement_bytes,
    bool fast_advertisement) {
  RunOnPcpHandlerThread(
      "p2p-ble-device-discovered",
      [this, client, &peripheral, service_id, advertisement_bytes,
       fast_advertisement]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering()) {
          NEARBY_LOGS(WARNING)
              << "Skipping discovery of BleAdvertisement header "
              << absl::BytesToHexString(advertisement_bytes.data())
              << " because we are no longer discovering.";
          return;
        }

        // Parse the BLE advertisement bytes.
        BleAdvertisement advertisement(fast_advertisement, advertisement_bytes);

        // Make sure the BLE advertisement points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedBleEndpoint(service_id, advertisement)) return;

        // Store all the state we need to be able to re-create a BleEndpoint
        // in BlePeripheralLostHandler, since that isn't privy to
        // the bytes of the ble advertisement itself.
        found_ble_endpoints_.emplace(
            peripheral.GetName(),
            BleEndpointState(advertisement.GetEndpointId(),
                             advertisement.GetEndpointInfo()));

        // Report the discovered endpoint to the client.
        NEARBY_LOGS(INFO) << "Found BleAdvertisement "
                          << absl::BytesToHexString(advertisement_bytes.data())
                          << " (with endpoint_id="
                          << advertisement.GetEndpointId()
                          << ", and endpoint_info="
                          << absl::BytesToHexString(
                                 advertisement.GetEndpointInfo().data())
                          << ").",
            OnEndpointFound(client,
                            std::make_shared<BleEndpoint>(BleEndpoint{
                                {advertisement.GetEndpointId(),
                                 advertisement.GetEndpointInfo(), service_id,
                                 proto::connections::Medium::BLE,
                                 advertisement.GetWebRtcState()},
                                peripheral,
                            }));

        // Make sure we can connect to this device via Classic Bluetooth.
        std::string remote_bluetooth_mac_address =
            advertisement.GetBluetoothMacAddress();
        if (remote_bluetooth_mac_address.empty()) {
          NEARBY_LOGS(INFO)
              << "No Bluetooth Classic MAC address found in advertisement.";
          return;
        }

        BluetoothDevice remote_bluetooth_device =
            bluetooth_medium_.GetRemoteDevice(remote_bluetooth_mac_address);
        if (!remote_bluetooth_device.IsValid()) {
          NEARBY_LOGS(INFO)
              << "A valid Bluetooth device could not be derived from the MAC "
                 "address "
              << remote_bluetooth_mac_address;
          return;
        }

        OnEndpointFound(client,
                        std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
                            {
                                advertisement.GetEndpointId(),
                                advertisement.GetEndpointInfo(),
                                service_id,
                                proto::connections::Medium::BLUETOOTH,
                                advertisement.GetWebRtcState(),
                            },
                            remote_bluetooth_device,
                        }));
      });
}

void P2pClusterPcpHandler::BlePeripheralLostHandler(
    ClientProxy* client, BlePeripheral& peripheral,
    const std::string& service_id) {
  std::string peripheral_name = peripheral.GetName();
  NEARBY_LOG(INFO, "Ble: [LOST, SCHED] peripheral_name=%s",
             peripheral_name.c_str());
  RunOnPcpHandlerThread(
      "p2p-ble-device-lost",
      [this, client, service_id, &peripheral]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering()) {
          NEARBY_LOGS(WARNING)
              << "Ignoring lost BlePeripheral " << peripheral.GetName()
              << " because we are no longer discovering.";
          return;
        }

        // Remove this BlePeripheral from found_ble_endpoints_, and
        // report the endpoint as lost to the client.
        auto item = found_ble_endpoints_.find(peripheral.GetName());
        if (item != found_ble_endpoints_.end()) {
          BleEndpointState ble_endpoint_state(item->second);
          found_ble_endpoints_.erase(item);

          // Report the discovered endpoint to the client.
          NEARBY_LOGS(INFO)
              << "Lost BleEndpoint for BlePeripheral " << peripheral.GetName()
              << " (with endpoint_id=" << ble_endpoint_state.endpoint_id
              << " and endpoint_info="
              << absl::BytesToHexString(ble_endpoint_state.endpoint_info.data())
              << ").";
          OnEndpointLost(client, DiscoveredEndpoint{
                                     ble_endpoint_state.endpoint_id,
                                     ble_endpoint_state.endpoint_info,
                                     service_id,
                                     proto::connections::Medium::BLE,
                                     WebRtcState::kUndefined,
                                 });
        }
      });
}

bool P2pClusterPcpHandler::IsRecognizedWifiLanEndpoint(
    const std::string& service_id,
    const WifiLanServiceInfo& wifi_lan_service_info) const {
  if (!wifi_lan_service_info.IsValid()) {
    NEARBY_LOGS(INFO)
        << "WifiLanServiceInfo doesn't conform to the format, discarding.";
    return false;
  }

  if (wifi_lan_service_info.GetPcp() != GetPcp()) {
    NEARBY_LOGS(INFO)
        << "WifiLanServiceInfo doesn't match on Pcp; expected "
        << PcpToStrategy(GetPcp()).GetName() << ", found "
        << PcpToStrategy(wifi_lan_service_info.GetPcp()).GetName();
    return false;
  }

  ByteArray expected_service_id_hash =
      GenerateHash(service_id, WifiLanServiceInfo::kServiceIdHashLength);

  if (wifi_lan_service_info.GetServiceIdHash() != expected_service_id_hash) {
    NEARBY_LOGS(INFO)
        << "WifiLanServiceInfo doesn't match on expected service_id_hash; "
           "expected "
        << absl::BytesToHexString(expected_service_id_hash.data()) << ", found "
        << absl::BytesToHexString(
               wifi_lan_service_info.GetServiceIdHash().data());
    return false;
  }

  return true;
}

void P2pClusterPcpHandler::WifiLanServiceDiscoveredHandler(
    ClientProxy* client, NsdServiceInfo service_info,
    const std::string& service_id) {
  RunOnPcpHandlerThread(
      "p2p-wifi-service-discovered",
      [this, client, service_id, service_info]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering()) {
          NEARBY_LOGS(WARNING) << "Skipping discovery of NsdServiceInfo "
                               << service_info.GetServiceName()
                               << " because we are no longer discovering.";
          return;
        }

        // Parse the WifiLanServiceInfo.
        WifiLanServiceInfo wifi_lan_service_info(service_info);
        // Make sure the WifiLan service name points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedWifiLanEndpoint(service_id, wifi_lan_service_info)) {
          return;
        }

        // Report the discovered endpoint to the client.
        NEARBY_LOGS(INFO) << "Found NsdServiceInfo "
                          << service_info.GetServiceName()
                          << " (with endpoint_id="
                          << wifi_lan_service_info.GetEndpointId()
                          << "and endpoint_info="
                          << absl::BytesToHexString(
                                 wifi_lan_service_info.GetEndpointInfo().data())
                          << ").";
        OnEndpointFound(client,
                        std::make_shared<WifiLanEndpoint>(WifiLanEndpoint{
                            {
                                wifi_lan_service_info.GetEndpointId(),
                                wifi_lan_service_info.GetEndpointInfo(),
                                service_id,
                                proto::connections::Medium::WIFI_LAN,
                                wifi_lan_service_info.GetWebRtcState(),
                            },
                            service_info,
                        }));
      });
}

void P2pClusterPcpHandler::WifiLanServiceLostHandler(
    ClientProxy* client, NsdServiceInfo service_info,
    const std::string& service_id) {
  NEARBY_LOGS(INFO) << "WifiLan: [LOST, SCHED] service_info=" << &service_info
                    << ", service_name=" << service_info.GetServiceName();
  RunOnPcpHandlerThread(
      "p2p-wifi-service-lost",
      [this, client, service_id, service_info]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering()) {
          NEARBY_LOGS(WARNING) << "Ignoring lost NsdServiceInfo "
                               << service_info.GetServiceName()
                               << " because we are no longer "
                                  "discovering.";
          return;
        }

        // Parse the WifiLanServiceInfo.
        WifiLanServiceInfo wifi_lan_service_info(service_info);

        // Make sure the WifiLan service name points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedWifiLanEndpoint(service_id, wifi_lan_service_info))
          return;

        // Report the lost endpoint to the client.
        NEARBY_LOGS(INFO) << "Lost NsdServiceInfo "
                          << service_info.GetServiceName()
                          << " (with endpoint_id="
                          << wifi_lan_service_info.GetEndpointId()
                          << " and endpoint_info="
                          << absl::BytesToHexString(
                                 wifi_lan_service_info.GetEndpointInfo().data())
                          << ").";
        OnEndpointLost(client, DiscoveredEndpoint{
                                   wifi_lan_service_info.GetEndpointId(),
                                   wifi_lan_service_info.GetEndpointInfo(),
                                   service_id,
                                   proto::connections::Medium::WIFI_LAN,
                                   WebRtcState::kUndefined,
                               });
      });
}

BasePcpHandler::StartOperationResult P2pClusterPcpHandler::StartDiscoveryImpl(
    ClientProxy* client, const std::string& service_id,
    const DiscoveryOptions& discovery_options) {
  // If this is an out-of-band connection, do not start actual discovery, since
  // this connection is intended to be completed via InjectEndpointImpl().
  if (discovery_options.is_out_of_band_connection) {
    return {.status = {Status::kSuccess},
            .mediums = discovery_options.allowed.GetMediums(true)};
  }

  std::vector<proto::connections::Medium> mediums_started_successfully;

  if (discovery_options.allowed.wifi_lan) {
    proto::connections::Medium wifi_lan_medium = StartWifiLanDiscovery(
        {
            .service_discovered_cb = absl::bind_front(
                &P2pClusterPcpHandler::WifiLanServiceDiscoveredHandler, this,
                client),
            .service_lost_cb = absl::bind_front(
                &P2pClusterPcpHandler::WifiLanServiceLostHandler, this, client),
        },
        client, service_id);
    if (wifi_lan_medium != proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOGS(INFO)
          << "P2pClusterPcpHandler::StartDiscoveryImpl: WifiLan added";
      mediums_started_successfully.push_back(wifi_lan_medium);
    }
  }

  if (discovery_options.allowed.bluetooth) {
    proto::connections::Medium bluetooth_medium = StartBluetoothDiscovery(
        {
            .device_discovered_cb = absl::bind_front(
                &P2pClusterPcpHandler::BluetoothDeviceDiscoveredHandler, this,
                client, service_id),
            .device_name_changed_cb = absl::bind_front(
                &P2pClusterPcpHandler::BluetoothNameChangedHandler, this,
                client, service_id),
            .device_lost_cb = absl::bind_front(
                &P2pClusterPcpHandler::BluetoothDeviceLostHandler, this, client,
                service_id),
        },
        client, service_id);
    if (bluetooth_medium != proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOG(INFO, "P2pClusterPcpHandler::StartDiscoveryImpl: BT added");
      mediums_started_successfully.push_back(bluetooth_medium);
      bluetooth_classic_discoverer_client_id_ = client->GetClientId();
    }
  }

  if (discovery_options.allowed.ble) {
    proto::connections::Medium ble_medium = StartBleScanning(
        {
            .peripheral_discovered_cb = absl::bind_front(
                &P2pClusterPcpHandler::BlePeripheralDiscoveredHandler, this,
                client),
            .peripheral_lost_cb = absl::bind_front(
                &P2pClusterPcpHandler::BlePeripheralLostHandler, this, client),
        },
        client, service_id, discovery_options.fast_advertisement_service_uuid);
    if (ble_medium != proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOG(INFO, "P2pClusterPcpHandler::StartDiscoveryImpl: Ble added");
      mediums_started_successfully.push_back(ble_medium);
    }
  }

  if (mediums_started_successfully.empty()) {
    NEARBY_LOGS(ERROR)
        << "Failed StartDiscovery() for client=" << client->GetClientId()
        << " because we couldn't scan on Bluetooth, BLE, or WifiLan for "
           "service_id="
        << service_id;
    return {
        .status = {Status::kBluetoothError},
    };
  }

  return {
      .status = {Status::kSuccess},
      .mediums = std::move(mediums_started_successfully),
  };
}

Status P2pClusterPcpHandler::StopDiscoveryImpl(ClientProxy* client) {
  wifi_lan_medium_.StopDiscovery(client->GetDiscoveryServiceId());
  if (client->GetClientId() == bluetooth_classic_discoverer_client_id_) {
    bluetooth_medium_.StopDiscovery();
    bluetooth_classic_discoverer_client_id_ = 0;
  } else {
    NEARBY_LOGS(INFO) << "Skipped BT StopDiscovery for client="
                      << client->GetClientId()
                      << ", client that started discovery is "
                      << bluetooth_classic_discoverer_client_id_;
  }

  ble_medium_.StopScanning(client->GetDiscoveryServiceId());
  return {Status::kSuccess};
}

Status P2pClusterPcpHandler::InjectEndpointImpl(
    ClientProxy* client, const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  NEARBY_LOGS(INFO) << "InjectEndpoint.";
  // Bluetooth is the only supported out-of-band connection medium.
  if (metadata.medium != Medium::BLUETOOTH) {
    NEARBY_LOGS(WARNING) << "InjectEndpointImpl: Only Bluetooth is supported.";
    return {Status::kError};
  }

  BluetoothDevice remote_bluetooth_device =
      injected_bluetooth_device_store_.CreateInjectedBluetoothDevice(
          metadata.remote_bluetooth_mac_address, metadata.endpoint_id,
          metadata.endpoint_info,
          GenerateHash(service_id, BluetoothDeviceName::kServiceIdHashLength),
          GetPcp());

  if (!remote_bluetooth_device.IsValid()) {
    NEARBY_LOG(WARNING, "InjectEndpointImpl: Invalid parameters.");
    return {Status::kError};
  }

  BluetoothDeviceDiscoveredHandler(client, service_id, remote_bluetooth_device);
  return {Status::kSuccess};
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::ConnectImpl(
    ClientProxy* client, BasePcpHandler::DiscoveredEndpoint* endpoint) {
  if (!endpoint) {
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kError},
    };
  }
  switch (endpoint->medium) {
    case proto::connections::Medium::BLUETOOTH: {
      auto* bluetooth_endpoint = down_cast<BluetoothEndpoint*>(endpoint);
      if (bluetooth_endpoint) {
        return BluetoothConnectImpl(client, bluetooth_endpoint);
      }
      break;
    }
    case proto::connections::Medium::BLE: {
      auto* ble_endpoint = down_cast<BleEndpoint*>(endpoint);
      if (ble_endpoint) {
        return BleConnectImpl(client, ble_endpoint);
      }
      break;
    }
    case proto::connections::Medium::WIFI_LAN: {
      auto* wifi_lan_endpoint = down_cast<WifiLanEndpoint*>(endpoint);
      if (wifi_lan_endpoint) {
        return WifiLanConnectImpl(client, wifi_lan_endpoint);
      }
      break;
    }
    case proto::connections::Medium::WEB_RTC: {
      break;
    }
    default:
      break;
  }

  return BasePcpHandler::ConnectImplResult{
      .status = {Status::kError},
  };
}

proto::connections::Medium P2pClusterPcpHandler::StartBluetoothAdvertising(
    ClientProxy* client, const std::string& service_id,
    const ByteArray& service_id_hash, const std::string& local_endpoint_id,
    const ByteArray& local_endpoint_info, WebRtcState web_rtc_state) {
  // Start listening for connections before advertising in case a connection
  // request comes in very quickly.
  NEARBY_LOG(
      INFO,
      "P2pClusterPcpHandler::StartBluetoothAdvertising: service=%s: start",
      service_id.c_str());
  if (!bluetooth_medium_.IsAcceptingConnections(service_id)) {
    if (!bluetooth_radio_.Enable() ||
        !bluetooth_medium_.StartAcceptingConnections(
            service_id, {.accepted_cb = [this, client, local_endpoint_info](
                                            const std::string& service_id,
                                            BluetoothSocket socket) {
              if (!socket.IsValid()) {
                NEARBY_LOGS(WARNING)
                    << "Invalid socket in accept callback("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId();
                return;
              }
              RunOnPcpHandlerThread(
                  "p2p-bt-on-incoming-connection",
                  [this, client, local_endpoint_info, service_id,
                   socket = std::move(socket)]()
                      RUN_ON_PCP_HANDLER_THREAD() mutable {
                        std::string remote_device_name =
                            socket.GetRemoteDevice().GetName();
                        auto channel =
                            absl::make_unique<BluetoothEndpointChannel>(
                                service_id, /*channel_name=*/remote_device_name,
                                socket);
                        ByteArray remote_device_info{remote_device_name};

                        OnIncomingConnection(
                            client, remote_device_info, std::move(channel),
                            proto::connections::Medium::BLUETOOTH);
                      });
            }})) {
      NEARBY_LOGS(WARNING)
          << "In StartBluetoothAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start listening for incoming Bluetooth "
             "connections to service_id="
          << service_id;
      return proto::connections::UNKNOWN_MEDIUM;
    }
    NEARBY_LOGS(INFO)
        << "In StartBluetoothAdvertising("
        << absl::BytesToHexString(local_endpoint_info.data())
        << "), client=" << client->GetClientId()
        << " started listening for incoming Bluetooth connections to "
           "service_id="
        << service_id;
  }

  // Generate a BluetoothDeviceName with which to become Bluetooth discoverable.
  // TODO(b/169550050): Implement UWBAddress.
  std::string device_name(BluetoothDeviceName(
      kBluetoothDeviceNameVersion, GetPcp(), local_endpoint_id, service_id_hash,
      local_endpoint_info, ByteArray{}, web_rtc_state));
  if (device_name.empty()) {
    NEARBY_LOGS(WARNING) << "In StartBluetoothAdvertising("
                         << absl::BytesToHexString(local_endpoint_info.data())
                         << "), client=" << client->GetClientId()
                         << " failed to generate BluetoothDeviceName {version="
                         << static_cast<int>(kBluetoothDeviceNameVersion)
                         << ", pcp=" << PcpToStrategy(GetPcp()).GetName()
                         << ", endpoint_id=" << local_endpoint_id
                         << ", service_id_hash="
                         << absl::BytesToHexString(service_id_hash.data())
                         << ", endpoint_info="
                         << absl::BytesToHexString(local_endpoint_info.data())
                         << "}.";
    bluetooth_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }
  NEARBY_LOGS(INFO) << "In StartBluetoothAdvertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId()
                    << " generated BluetoothDeviceName " << device_name
                    << " with service_id=" << service_id;

  // Become Bluetooth discoverable.
  if (!bluetooth_medium_.TurnOnDiscoverability(device_name)) {
    NEARBY_LOGS(INFO)
        << "In StartBluetoothAdvertising("
        << absl::BytesToHexString(local_endpoint_info.data())
        << "), client=" << client->GetClientId()
        << " couldn't start Bluetooth advertising with BluetoothDeviceName "
        << device_name;
    bluetooth_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }
  NEARBY_LOGS(INFO)
      << "In StartBluetoothAdvertising("
      << absl::BytesToHexString(local_endpoint_info.data())
      << "), client=" << client->GetClientId()
      << " started Bluetooth advertising with BluetoothDeviceName "
      << device_name;
  return proto::connections::BLUETOOTH;
}

proto::connections::Medium P2pClusterPcpHandler::StartBluetoothDiscovery(
    BluetoothDiscoveredDeviceCallback callback, ClientProxy* client,
    const std::string& service_id) {
  if (bluetooth_radio_.Enable() &&
      bluetooth_medium_.StartDiscovery(std::move(callback))) {
    NEARBY_LOGS(INFO) << "In StartBluetoothDiscovery(), client="
                      << client->GetClientId()
                      << " started scanning for Bluetooth for service_id="
                      << service_id;
    return proto::connections::BLUETOOTH;
  } else {
    NEARBY_LOGS(INFO) << "In StartBluetoothDiscovery(), client="
                      << client->GetClientId()
                      << " couldn't start scanning on Bluetooth for service_id="
                      << service_id;
    return proto::connections::UNKNOWN_MEDIUM;
  }
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::BluetoothConnectImpl(
    ClientProxy* client, BluetoothEndpoint* endpoint) {
  NEARBY_LOGS(VERBOSE) << "Client " << client->GetClientId()
                       << " is attempting to connect to endpoint(id="
                       << endpoint->endpoint_id << ") over Bluetooth Classic.";
  BluetoothDevice& device = endpoint->bluetooth_device;

  BluetoothSocket bluetooth_socket = bluetooth_medium_.Connect(
      device, endpoint->service_id,
      client->GetCancellationFlag(endpoint->endpoint_id));
  if (!bluetooth_socket.IsValid()) {
    NEARBY_LOGS(ERROR)
        << "In BluetoothConnectImpl(), failed to connect to Bluetooth device "
        << device.GetName() << " for endpoint(id=" << endpoint->endpoint_id
        << ").";
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kBluetoothError},
    };
  }

  auto channel = absl::make_unique<BluetoothEndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id,
      bluetooth_socket);
  NEARBY_LOGS(VERBOSE) << "Client" << client->GetClientId()
                       << " created Bluetooth endpoint channel to endpoint(id="
                       << endpoint->endpoint_id << ").";
  return BasePcpHandler::ConnectImplResult{
      .medium = proto::connections::Medium::BLUETOOTH,
      .status = {Status::kSuccess},
      .endpoint_channel = std::move(channel),
  };
}

proto::connections::Medium P2pClusterPcpHandler::StartBleAdvertising(
    ClientProxy* client, const std::string& service_id,
    const std::string& local_endpoint_id, const ByteArray& local_endpoint_info,
    const AdvertisingOptions& advertising_options, WebRtcState web_rtc_state) {
  bool fast_advertisement =
      !advertising_options.fast_advertisement_service_uuid.empty();
  PowerLevel power_level = advertising_options.low_power
                               ? PowerLevel::kLowPower
                               : PowerLevel::kHighPower;

  // Start listening for connections before advertising in case a connection
  // request comes in very quickly. BLE allows connecting over BLE itself, as
  // well as advertising the Bluetooth MAC address to allow connecting over
  // Bluetooth Classic.
  NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::StartBleAdvertising: service_id="
                    << service_id << " : start";
  if (!ble_medium_.IsAcceptingConnections(service_id)) {
    if (!bluetooth_radio_.Enable() ||
        !ble_medium_.StartAcceptingConnections(
            service_id, {.accepted_cb = [this, client, local_endpoint_info](
                                            BleSocket socket,
                                            const std::string& service_id) {
              if (!socket.IsValid()) {
                NEARBY_LOGS(WARNING)
                    << "Invalid socket in accept callback("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId();
                return;
              }
              RunOnPcpHandlerThread(
                  "p2p-ble-on-incoming-connection",
                  [this, client, local_endpoint_info, service_id,
                   socket = std::move(socket)]()
                      RUN_ON_PCP_HANDLER_THREAD() mutable {
                        std::string remote_peripheral_name =
                            socket.GetRemotePeripheral().GetName();
                        auto channel = absl::make_unique<BleEndpointChannel>(
                            service_id,
                            /*channel_name=*/remote_peripheral_name, socket);
                        ByteArray remote_peripheral_info =
                            socket.GetRemotePeripheral().GetAdvertisementBytes(
                                service_id);

                        OnIncomingConnection(client, remote_peripheral_info,
                                             std::move(channel),
                                             proto::connections::Medium::BLE);
                      });
            }})) {
      NEARBY_LOGS(WARNING)
          << "In StartBleAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start accepting for incoming BLE connections to "
             "service_id="
          << service_id;
      return proto::connections::UNKNOWN_MEDIUM;
    }
    NEARBY_LOGS(INFO)
        << "In StartBleAdvertising("
        << absl::BytesToHexString(local_endpoint_info.data())
        << "), client=" << client->GetClientId()
        << " started accepting for incoming BLE connections to service_id="
        << service_id;
  }

  if (ShouldAdvertiseBluetoothMacOverBle(power_level) ||
      ShouldAcceptBluetoothConnections(advertising_options)) {
    if (bluetooth_medium_.IsAvailable() &&
        !bluetooth_medium_.IsAcceptingConnections(service_id)) {
      if (!bluetooth_radio_.Enable() ||
          !bluetooth_medium_.StartAcceptingConnections(
              service_id, {.accepted_cb = [this, client, local_endpoint_info](
                                              const std::string& service_id,
                                              BluetoothSocket socket) {
                if (!socket.IsValid()) {
                  NEARBY_LOGS(WARNING)
                      << "In BT StartAcceptingConnections.accepted_cb("
                      << absl::BytesToHexString(local_endpoint_info.data())
                      << "), client=" << client->GetClientId()
                      << ": Invalid socket in accept callback.";
                  return;
                }
                RunOnPcpHandlerThread(
                    "p2p-bt-on-incoming-connection",
                    [this, client, local_endpoint_info, service_id,
                     socket = std::move(socket)]()
                        RUN_ON_PCP_HANDLER_THREAD() mutable {
                          std::string remote_device_name =
                              socket.GetRemoteDevice().GetName();
                          auto channel =
                              absl::make_unique<BluetoothEndpointChannel>(
                                  service_id,
                                  /*channel_name=*/remote_device_name, socket);
                          ByteArray remote_device_info{remote_device_name};

                          OnIncomingConnection(
                              client, remote_device_info, std::move(channel),
                              proto::connections::Medium::BLUETOOTH);
                        });
              }})) {
        NEARBY_LOGS(WARNING)
            << "In BT StartBleAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " failed to start accepting for incoming BLE connections to "
               "service_id="
            << service_id;
        ble_medium_.StopAcceptingConnections(service_id);
        return proto::connections::UNKNOWN_MEDIUM;
      }
      NEARBY_LOGS(INFO)
          << "In BT StartBleAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " started accepting for incoming BLE connections to service_id="
          << service_id;
    }
  }

  NEARBY_LOGS(INFO) << "In StartBleAdvertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId()
                    << " start to generate BleAdvertisement with service_id="
                    << service_id
                    << ", local endpoint_id=" << local_endpoint_id;
  // Generate a BleAdvertisement. If a fast advertisement service UUID was
  // provided, create a fast BleAdvertisement.
  ByteArray advertisement_bytes;
  // TODO(b/169550050): Implement UWBAddress.
  if (fast_advertisement) {
    advertisement_bytes = ByteArray(
        BleAdvertisement(kBleAdvertisementVersion, GetPcp(), local_endpoint_id,
                         local_endpoint_info, ByteArray{}));
  } else {
    const ByteArray service_id_hash =
        GenerateHash(service_id, BleAdvertisement::kServiceIdHashLength);
    std::string bluetooth_mac_address;
    if (bluetooth_medium_.IsAvailable() &&
        ShouldAdvertiseBluetoothMacOverBle(power_level))
      bluetooth_mac_address = bluetooth_medium_.GetMacAddress();

    advertisement_bytes = ByteArray(
        BleAdvertisement(kBleAdvertisementVersion, GetPcp(), service_id_hash,
                         local_endpoint_id, local_endpoint_info,
                         bluetooth_mac_address, ByteArray{}, web_rtc_state));
  }
  if (advertisement_bytes.Empty()) {
    NEARBY_LOGS(WARNING) << "In StartBleAdvertising("
                         << absl::BytesToHexString(local_endpoint_info.data())
                         << "), client=" << client->GetClientId()
                         << " failed to create an advertisement.",
        ble_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }

  NEARBY_LOGS(INFO) << "In StartBleAdvertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId()
                    << " generated BleAdvertisement with service_id="
                    << service_id;

  if (!ble_medium_.StartAdvertising(
          service_id, advertisement_bytes,
          advertising_options.fast_advertisement_service_uuid)) {
    NEARBY_LOGS(WARNING)
        << "In StartBleAdvertising("
        << absl::BytesToHexString(local_endpoint_info.data())
        << "), client=" << client->GetClientId()
        << " couldn't start BLE Advertising with BleAdvertisement "
        << absl::BytesToHexString(advertisement_bytes.data());
    ble_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }
  NEARBY_LOGS(INFO) << "In startBleAdvertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId()
                    << " started BLE Advertising with BleAdvertisement "
                    << absl::BytesToHexString(advertisement_bytes.data());
  return proto::connections::BLE;
}

proto::connections::Medium P2pClusterPcpHandler::StartBleScanning(
    BleDiscoveredPeripheralCallback callback, ClientProxy* client,
    const std::string& service_id,
    const std::string& fast_advertisement_service_uuid) {
  if (bluetooth_radio_.Enable() &&
      ble_medium_.StartScanning(service_id, fast_advertisement_service_uuid,
                                std::move(callback))) {
    NEARBY_LOGS(INFO)
        << "In StartBleScanning(), client=" << client->GetClientId()
        << " started scanning for BLE advertisements for service_id="
        << service_id;
    return proto::connections::BLE;
  } else {
    NEARBY_LOGS(INFO) << "In StartBleScanning(), client="
                      << client->GetClientId()
                      << " couldn't start scanning on BLE for service_id="
                      << service_id;
    return proto::connections::UNKNOWN_MEDIUM;
  }
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::BleConnectImpl(
    ClientProxy* client, BleEndpoint* endpoint) {
  NEARBY_LOGS(VERBOSE) << "Client " << client->GetClientId()
                       << " is attempting to connect to endpoint(id="
                       << endpoint->endpoint_id << ") over BLE.";

  BlePeripheral& peripheral = endpoint->ble_peripheral;

  BleSocket ble_socket =
      ble_medium_.Connect(peripheral, endpoint->service_id,
                          client->GetCancellationFlag(endpoint->endpoint_id));
  if (!ble_socket.IsValid()) {
    NEARBY_LOGS(ERROR)
        << "In BleConnectImpl(), failed to connect to BLE device "
        << peripheral.GetName() << " for endpoint(id=" << endpoint->endpoint_id
        << ").";
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kBleError},
    };
  }

  auto channel = absl::make_unique<BleEndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id, ble_socket);

  return BasePcpHandler::ConnectImplResult{
      .medium = proto::connections::Medium::BLE,
      .status = {Status::kSuccess},
      .endpoint_channel = std::move(channel),
  };
}

proto::connections::Medium P2pClusterPcpHandler::StartWifiLanAdvertising(
    ClientProxy* client, const std::string& service_id,
    const std::string& local_endpoint_id, const ByteArray& local_endpoint_info,
    WebRtcState web_rtc_state) {
  // Start listening for connections before advertising in case a connection
  // request comes in very quickly.
  NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::StartWifiLanAdvertising: service="
                    << service_id << ": start";
  if (!wifi_lan_medium_.IsAcceptingConnections(service_id)) {
    if (!wifi_lan_medium_.StartAcceptingConnections(
            service_id,
            {.accepted_cb = [this, client, local_endpoint_info,
                             local_endpoint_id](const std::string& service_id,
                                                WifiLanSocket socket) {
              if (!socket.IsValid()) {
                NEARBY_LOGS(WARNING)
                    << "Invalid socket in accept callback("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId();
                return;
              }
              RunOnPcpHandlerThread(
                  "p2p-wifi-on-incoming-connection",
                  [this, client, local_endpoint_id, local_endpoint_info,
                   service_id,
                   socket = std::move(
                       socket)]() RUN_ON_PCP_HANDLER_THREAD() mutable {
                    std::string remote_service_name = local_endpoint_id;
                    auto channel = absl::make_unique<WifiLanEndpointChannel>(
                        service_id, /*channel_name=*/remote_service_name,
                        socket);
                    ByteArray remote_service_name_byte{remote_service_name};

                    OnIncomingConnection(client, remote_service_name_byte,
                                         std::move(channel),
                                         proto::connections::Medium::WIFI_LAN);
                  });
            }})) {
      NEARBY_LOGS(WARNING)
          << "In StartWifiLanAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start listening for incoming WifiLan connections "
             "to service_id="
          << service_id;
      return proto::connections::UNKNOWN_MEDIUM;
    }
    NEARBY_LOGS(INFO) << "In StartWifiLanAdvertising("
                      << absl::BytesToHexString(local_endpoint_info.data())
                      << "), client=" << client->GetClientId()
                      << " started listening for incoming WifiLan connections "
                         "to service_id = "
                      << service_id;
  }

  // Generate a WifiLanServiceInfo with which to become WifiLan discoverable.
  // TODO(b/169550050): Implement UWBAddress.
  const ByteArray service_id_hash =
      GenerateHash(service_id, WifiLanServiceInfo::kServiceIdHashLength);
  WifiLanServiceInfo service_info{kWifiLanServiceInfoVersion,
                                  GetPcp(),
                                  local_endpoint_id,
                                  service_id_hash,
                                  local_endpoint_info,
                                  ByteArray{},
                                  web_rtc_state};
  NsdServiceInfo nsd_service_info(service_info);
  if (!nsd_service_info.IsValid()) {
    NEARBY_LOGS(WARNING) << "In StartWifiLanAdvertising("
                         << absl::BytesToHexString(local_endpoint_info.data())
                         << "), client=" << client->GetClientId()
                         << " failed to generate WifiLanServiceInfo {version="
                         << static_cast<int>(kWifiLanServiceInfoVersion)
                         << ", pcp=" << PcpToStrategy(GetPcp()).GetName()
                         << ", endpoint_id=" << local_endpoint_id
                         << ", service_id_hash="
                         << absl::BytesToHexString(service_id_hash.data())
                         << ", endpoint_info="
                         << absl::BytesToHexString(local_endpoint_info.data())
                         << "}.";
    wifi_lan_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }
  NEARBY_LOGS(INFO) << "In StartWifiLanAdvertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId()
                    << " generated WifiLanServiceInfo "
                    << nsd_service_info.GetServiceName()
                    << " with service_id=" << service_id;

  if (!wifi_lan_medium_.StartAdvertising(service_id, nsd_service_info)) {
    NEARBY_LOGS(INFO) << "In StartWifiLanAdvertising("
                      << absl::BytesToHexString(local_endpoint_info.data())
                      << "), client=" << client->GetClientId()
                      << " couldn't advertise with WifiLanServiceInfo "
                      << nsd_service_info.GetServiceName();
    wifi_lan_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }
  NEARBY_LOGS(INFO) << "In StartWifiLanAdvertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId()
                    << " advertised with WifiLanServiceInfo "
                    << nsd_service_info.GetServiceName();
  return proto::connections::WIFI_LAN;
}

proto::connections::Medium P2pClusterPcpHandler::StartWifiLanDiscovery(
    WifiLanDiscoveredServiceCallback callback, ClientProxy* client,
    const std::string& service_id) {
  if (wifi_lan_medium_.StartDiscovery(service_id, std::move(callback))) {
    NEARBY_LOGS(INFO) << "In StartWifiLanDiscovery(), client="
                      << client->GetClientId()
                      << " started scanning for Wifi devices for service_id="
                      << service_id;
    return proto::connections::WIFI_LAN;
  } else {
    NEARBY_LOGS(INFO) << "In StartWifiLanDiscovery(), client="
                      << client->GetClientId()
                      << " couldn't start scanning on Wifi for service_id="
                      << service_id;
    return proto::connections::UNKNOWN_MEDIUM;
  }
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::WifiLanConnectImpl(
    ClientProxy* client, WifiLanEndpoint* endpoint) {
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " is attempting to connect to endpoint(id="
                    << endpoint->endpoint_id << ") over WifiLan.";
  WifiLanSocket socket = wifi_lan_medium_.Connect(
      endpoint->service_id, endpoint->service_info,
      client->GetCancellationFlag(endpoint->endpoint_id));
  NEARBY_LOGS(ERROR) << "In WifiLanConnectImpl(), connect to service "
                     << " socket=" << &socket.GetImpl()
                     << " for endpoint(id=" << endpoint->endpoint_id << ").";
  if (!socket.IsValid()) {
    NEARBY_LOGS(ERROR)
        << "In WifiLanConnectImpl(), failed to connect to service "
        << endpoint->service_info.GetServiceName()
        << " for endpoint(id=" << endpoint->endpoint_id << ").";
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kWifiLanError},
    };
  }

  auto channel = absl::make_unique<WifiLanEndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id, socket);
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " created WifiLan endpoint channel to endpoint(id="
                    << endpoint->endpoint_id << ").";
  return BasePcpHandler::ConnectImplResult{
      .medium = proto::connections::Medium::WIFI_LAN,
      .status = {Status::kSuccess},
      .endpoint_channel = std::move(channel),
  };
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
