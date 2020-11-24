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

#include "core/internal/p2p_cluster_pcp_handler.h"

#include "core/internal/base_pcp_handler.h"
#include "core/internal/ble_advertisement.h"
#include "core/internal/ble_endpoint_channel.h"
#include "core/internal/bluetooth_endpoint_channel.h"
#include "core/internal/bwu_manager.h"
#include "core/internal/mediums/utils.h"
#include "core/internal/mediums/webrtc/webrtc_socket_wrapper.h"
#include "core/internal/webrtc_endpoint_channel.h"
#include "core/internal/wifi_lan_endpoint_channel.h"
#include "platform/base/nsd_service_info.h"
#include "platform/base/types.h"
#include "platform/public/crypto.h"
#include "proto/connections_enums.pb.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/escaping.h"

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
    const ConnectionOptions& options) {
  return options.enable_bluetooth_listening;
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
    const ConnectionOptions& options) {
  std::vector<proto::connections::Medium> mediums_started_successfully;

  WebRtcState web_rtc_state{WebRtcState::kUnconnectable};

  if (options.allowed.wifi_lan) {
    const ByteArray wifi_lan_hash =
        GenerateHash(service_id, WifiLanServiceInfo::kServiceIdHashLength);
    proto::connections::Medium wifi_lan_medium = StartWifiLanAdvertising(
        client, service_id, wifi_lan_hash, local_endpoint_id,
        local_endpoint_info, web_rtc_state);
    if (wifi_lan_medium != proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOG(INFO,
                 "P2pClusterPcpHandler::StartAdvertisingImpl: WifiLan added");
      mediums_started_successfully.push_back(wifi_lan_medium);
    }
  }

  if (options.allowed.bluetooth) {
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

  if (options.allowed.ble) {
    proto::connections::Medium ble_medium =
        StartBleAdvertising(client, service_id, local_endpoint_id,
                            local_endpoint_info, options, web_rtc_state);
    if (ble_medium != proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOG(INFO, "P2pClusterPcpHandler::StartAdvertisingImpl: Ble added");
      mediums_started_successfully.push_back(ble_medium);
    }
  }

  if (mediums_started_successfully.empty()) {
    NEARBY_LOG(INFO, "P2pClusterPcpHandler::StartAdvertisingImpl: not started");
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
  } else {
    NEARBY_LOG(INFO,
               "Skipped BT TurnOffDiscoverability for client %d, client that "
               "turned on discoverability is %d",
               client->GetClientId(), bluetooth_classic_advertiser_client_id_);
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
    NEARBY_LOG(
        INFO,
        "P2pClusterPcpHandler::IsRecognizedBluetoothEndpoint: name is invalid");
    return false;
  }

  if (name.GetPcp() != GetPcp()) {
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::IsRecognizedBluetoothEndpoint: Pcp is "
               "not matched; name.Pcp=%d, Pcp=%d",
               name.GetPcp(), GetPcp());
    return false;
  }

  ByteArray expected_service_id_hash =
      GenerateHash(service_id, BluetoothDeviceName::kServiceIdHashLength);

  if (name.GetServiceIdHash() != expected_service_id_hash) {
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::IsRecognizedBluetoothEndpoint: service "
               "id hash is "
               "not matched; name.service_id_hash=%s, expected=%s",
               name.GetServiceIdHash().data(), expected_service_id_hash.data());
    return false;
  }

  return true;
}

void P2pClusterPcpHandler::BluetoothDeviceDiscoveredHandler(
    ClientProxy* client, const std::string& service_id,
    BluetoothDevice device) {
  RunOnPcpHandlerThread([this, client, service_id, device]() {
    // Make sure we are still discovering before proceeding.
    if (!client->IsDiscovering()) {
      NEARBY_LOG(INFO,
                 "BT discovery handler (FOUND) [client=%p, service=%s]: not "
                 "in discovery mode",
                 client, service_id.c_str());
      return;
    }

    // Parse the Bluetooth device name.
    const std::string device_name_string = device.GetName();
    BluetoothDeviceName device_name(device_name_string);

    // Make sure the Bluetooth device name points to a valid
    // endpoint we're discovering.
    if (!IsRecognizedBluetoothEndpoint(device_name_string, service_id,
                                       device_name))
      return;

    // Report the discovered endpoint to the client.
    NEARBY_LOGS(INFO)
        << "Invoking BasePcpHandler::OnEndpointFound() for BT service="
        << service_id << "; id=" << device_name.GetEndpointId() << "; name="
        << absl::BytesToHexString(device_name.GetEndpointInfo().data());
    OnEndpointFound(
        client, std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
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
  RunOnPcpHandlerThread([this, client, service_id, device_name_string]() {
    // Make sure we are still discovering before proceeding.
    if (!client->IsDiscovering()) {
      NEARBY_LOG(INFO,
                 "BT discovery handler (LOST) [client=%p, service=%s]: not "
                 "in discovery mode",
                 client, service_id.c_str());
      return;
    }

    // Parse the Bluetooth device name.
    BluetoothDeviceName device_name(device_name_string);

    // Make sure the Bluetooth device name points to a valid
    // endpoint we're discovering.
    if (!IsRecognizedBluetoothEndpoint(device_name_string, service_id,
                                       device_name))
      return;

    // Report the discovered endpoint to the client.
    NEARBY_LOG(INFO,
               "BT discovery handler (LOST) [client=%p, service=%s]: report "
               "to client",
               client, service_id.c_str());
    OnEndpointLost(client,
                   DiscoveredEndpoint{device_name.GetEndpointId(),
                                      device_name.GetEndpointInfo(), service_id,
                                      proto::connections::Medium::BLUETOOTH,
                                      WebRtcState::kUndefined});
  });
}

bool P2pClusterPcpHandler::IsRecognizedBleEndpoint(
    const std::string& service_id,
    const BleAdvertisement& advertisement) const {
  if (!advertisement.IsValid()) {
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::IsRecognizedBleEndpoint: advertisement "
               "is invalid");
    return false;
  }

  if (advertisement.GetVersion() != kBleAdvertisementVersion) {
    NEARBY_LOG(
        INFO,
        "P2pClusterPcpHandler::IsRecognizedBluetoothEndpoint: Version is "
        "not matched; advertisement.Version=%d, Version=%d",
        advertisement.GetVersion(), kBleAdvertisementVersion);
    return false;
  }

  if (advertisement.GetPcp() != GetPcp()) {
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::IsRecognizedBluetoothEndpoint: Pcp is "
               "not matched; advertisement.Pcp=%d, Pcp=%d",
               advertisement.GetPcp(), GetPcp());
    return false;
  }

  // Check ServiceId for normal advertisement.
  // ServiceIdHash is empty for fast advertisement.
  if (!advertisement.IsFastAdvertisement()) {
    ByteArray expected_service_id_hash =
        GenerateHash(service_id, BleAdvertisement::kServiceIdHashLength);

    if (advertisement.GetServiceIdHash() != expected_service_id_hash) {
      NEARBY_LOG(INFO,
                 "P2pClusterPcpHandler::IsRecognizedBleEndpoint: service "
                 "id hash is "
                 "not matched; advertisement.service_id_hash=%s, expected=%s",
                 advertisement.GetServiceIdHash().data(),
                 expected_service_id_hash.data());
      return false;
    }
  }

  return true;
}

void P2pClusterPcpHandler::BlePeripheralDiscoveredHandler(
    ClientProxy* client, BlePeripheral& peripheral,
    const std::string& service_id, const ByteArray& advertisement_bytes,
    bool fast_advertisement) {
  RunOnPcpHandlerThread([this, client, &peripheral, service_id,
                         advertisement_bytes, fast_advertisement]() {
    // Make sure we are still discovering before proceeding.
    if (!client->IsDiscovering()) {
      NEARBY_LOG(INFO,
                 "Ble scanning handler (FOUND) [client=%p, service_id=%s]: not "
                 "in discovery mode",
                 client, service_id.c_str());
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
    NEARBY_LOGS(INFO)
        << "Invoking BasePcpHandler::OnEndpointFound() for Ble service="
        << service_id << "; id=" << advertisement.GetEndpointId() << "; name="
        << absl::BytesToHexString(advertisement.GetEndpointInfo().data());
    OnEndpointFound(client, std::make_shared<BleEndpoint>(BleEndpoint{
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
          << "No Bluetooth Classic MAC address found in advertisement";
      return;
    }

    BluetoothDevice remote_bluetooth_device =
        bluetooth_medium_.GetRemoteDevice(remote_bluetooth_mac_address);
    if (!remote_bluetooth_device.IsValid()) {
      NEARBY_LOGS(INFO) << "A valid Bluetooth device could not be derived from "
                           "the MAC address "
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
  RunOnPcpHandlerThread([this, client, service_id, &peripheral]() {
    // Make sure we are still discovering before proceeding.
    if (!client->IsDiscovering()) {
      NEARBY_LOG(INFO,
                 "Ble scanning handler (LOST) [client=%p, service_id=%s]: not "
                 "in scanning mode",
                 client, service_id.c_str());
      return;
    }

    // Remove this BlePeripheral from found_ble_endpoints_, and
    // report the endpoint as lost to the client.
    auto item = found_ble_endpoints_.find(peripheral.GetName());
    if (item != found_ble_endpoints_.end()) {
      BleEndpointState ble_endpoint_state(item->second);
      found_ble_endpoints_.erase(item);

      // Report the discovered endpoint to the client.
      NEARBY_LOG(INFO,
                 "Ble scanning handler (LOST) [client=%p, "
                 "service_id=%s]: report to client",
                 client, service_id.c_str());
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
    const WifiLanServiceInfo& service_info) const {
  if (!service_info.IsValid()) {
    NEARBY_LOG(
        INFO,
        "P2pClusterPcpHandler::IsRecognizedWifiLanEndpoint: name is invalid");
    return false;
  }

  if (service_info.GetPcp() != GetPcp()) {
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::IsRecognizedWifiLanEndpoint: Pcp is "
               "not matched; name.Pcp=%d, Pcp=%d",
               service_info.GetPcp(), GetPcp());
    return false;
  }

  ByteArray expected_service_id_hash =
      GenerateHash(service_id, BluetoothDeviceName::kServiceIdHashLength);

  if (service_info.GetServiceIdHash() != expected_service_id_hash) {
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::IsRecognizedWifiLanEndpoint: service "
               "id hash is "
               "not matched; name.service_id_hash=%s, expected=%s",
               service_info.GetServiceIdHash().data(),
               expected_service_id_hash.data());
    return false;
  }

  return true;
}

void P2pClusterPcpHandler::WifiLanServiceDiscoveredHandler(
    ClientProxy* client, WifiLanService& wifi_lan_service,
    const std::string& service_id) {
  RunOnPcpHandlerThread([this, client, service_id, &wifi_lan_service]() {
    // Make sure we are still discovering before proceeding.
    if (!client->IsDiscovering()) {
      NEARBY_LOG(
          INFO,
          "WifiLan discovery handler (FOUND) [client=%p, service=%s]: not "
          "in discovery mode",
          client, service_id.c_str());
      return;
    }

    // Parse the WifiLanServiceInfo.
    WifiLanServiceInfo service_info(wifi_lan_service.GetServiceInfo());

    // Make sure the WifiLan service name points to a valid
    // endpoint we're discovering.
    if (!IsRecognizedWifiLanEndpoint(service_id, service_info)) return;

    // Report the discovered endpoint to the client.
    NEARBY_LOG(
        INFO,
        "Invoking BasePcpHandler::OnEndpointFound() for WifiLan "
        "service_id=%s; endpoint_id=%s; endpoint_info=%s",
        service_id.c_str(), service_info.GetEndpointId().c_str(),
        absl::BytesToHexString(service_info.GetEndpointInfo().data()).c_str());
    OnEndpointFound(client, std::make_shared<WifiLanEndpoint>(WifiLanEndpoint{
                                {
                                    service_info.GetEndpointId(),
                                    service_info.GetEndpointInfo(),
                                    service_id,
                                    proto::connections::Medium::WIFI_LAN,
                                    service_info.GetWebRtcState(),
                                },
                                wifi_lan_service,
                            }));
  });
}

void P2pClusterPcpHandler::WifiLanServiceLostHandler(
    ClientProxy* client, WifiLanService& wifi_lan_service,
    const std::string& service_id) {
  NsdServiceInfo nsd_service_info = wifi_lan_service.GetServiceInfo();
  NEARBY_LOG(INFO,
             "WifiLan: [LOST, SCHED] wifi_lan_service=%p, service_info_name=%s",
             &wifi_lan_service, nsd_service_info.GetServiceInfoName().c_str());
  RunOnPcpHandlerThread([this, client, service_id, nsd_service_info]() {
    // Make sure we are still discovering before proceeding.
    if (!client->IsDiscovering()) {
      NEARBY_LOG(
          INFO,
          "WifiLan discovery handler (LOST) [client=%p, service=%s]: not "
          "in discovery mode",
          client, service_id.c_str());
      return;
    }

    // Parse the WifiLanServiceInfo.
    WifiLanServiceInfo service_info(nsd_service_info);

    // Make sure the WifiLan service name points to a valid
    // endpoint we're discovering.
    if (!IsRecognizedWifiLanEndpoint(service_id, service_info)) return;

    // Report the discovered endpoint to the client.
    NEARBY_LOG(
        INFO,
        "WifiLan discovery handler (LOST) [client=%p, service_id=%s]: report "
        "to client",
        client, service_id.c_str());
    OnEndpointLost(client, DiscoveredEndpoint{
                               service_info.GetEndpointId(),
                               service_info.GetEndpointInfo(),
                               service_id,
                               proto::connections::Medium::WIFI_LAN,
                               WebRtcState::kUndefined,
                           });
  });
}

BasePcpHandler::StartOperationResult P2pClusterPcpHandler::StartDiscoveryImpl(
    ClientProxy* client, const std::string& service_id,
    const ConnectionOptions& options) {
  // If this is an out-of-band connection, do not start actual discovery, since
  // this connection is intended to be completed via InjectEndpointImpl().
  if (options.is_out_of_band_connection) {
    return {.status = {Status::kSuccess},
            .mediums = options.allowed.GetMediums(true)};
  }

  std::vector<proto::connections::Medium> mediums_started_successfully;

  if (options.allowed.wifi_lan) {
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
      NEARBY_LOG(INFO,
                 "P2pClusterPcpHandler::StartDiscoveryImpl: WifiLan added");
      mediums_started_successfully.push_back(wifi_lan_medium);
    }
  }

  if (options.allowed.bluetooth) {
    proto::connections::Medium bluetooth_medium = StartBluetoothDiscovery(
        {
            .device_discovered_cb = absl::bind_front(
                &P2pClusterPcpHandler::BluetoothDeviceDiscoveredHandler, this,
                client, service_id),
            .device_name_changed_cb = absl::bind_front(
                &P2pClusterPcpHandler::BluetoothDeviceDiscoveredHandler, this,
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

  if (options.allowed.ble) {
    proto::connections::Medium ble_medium = StartBleScanning(
        {
            .peripheral_discovered_cb = absl::bind_front(
                &P2pClusterPcpHandler::BlePeripheralDiscoveredHandler, this,
                client),
            .peripheral_lost_cb = absl::bind_front(
                &P2pClusterPcpHandler::BlePeripheralLostHandler, this, client),
        },
        client, service_id, options.fast_advertisement_service_uuid);
    if (ble_medium != proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOG(INFO, "P2pClusterPcpHandler::StartDiscoveryImpl: Ble added");
      mediums_started_successfully.push_back(ble_medium);
    }
  }

  if (mediums_started_successfully.empty()) {
    NEARBY_LOG(INFO, "P2pClusterPcpHandler::StartDiscoveryImpl: nothing added");
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
  } else {
    NEARBY_LOG(INFO,
               "Skipped BT stopDiscovery for client %d, client that started "
               "discovery is %d",
               client->GetClientId(), bluetooth_classic_discoverer_client_id_);
  }
  ble_medium_.StopScanning(client->GetDiscoveryServiceId());
  return {Status::kSuccess};
}

Status P2pClusterPcpHandler::InjectEndpointImpl(
    ClientProxy* client, const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  NEARBY_LOG(INFO, "InjectEndpoint");
  // Bluetooth is the only supported out-of-band connection medium.
  if (metadata.medium != Medium::BLUETOOTH) {
    NEARBY_LOG(WARNING, "InjectEndpointImpl: Only Bluetooth is supported");
    return {Status::kError};
  }

  BluetoothDevice remote_bluetooth_device =
      injected_bluetooth_device_store_.CreateInjectedBluetoothDevice(
          metadata.remote_bluetooth_mac_address, metadata.endpoint_id,
          metadata.endpoint_info,
          GenerateHash(service_id, BluetoothDeviceName::kServiceIdHashLength),
          GetPcp());

  if (!remote_bluetooth_device.IsValid()) {
    NEARBY_LOG(WARNING, "InjectEndpointImpl: Invalid parameters");
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
  if (bluetooth_medium_.IsAcceptingConnections(service_id)) {
    NEARBY_LOG(INFO, "BT is already accepting connections for service=%s",
               service_id.c_str());
    return proto::connections::UNKNOWN_MEDIUM;
  }

  NEARBY_LOG(
      INFO,
      "P2pClusterPcpHandler::StartBluetoothAdvertising: service=%s: invoking",
      service_id.c_str());
  if (!bluetooth_radio_.Enable() ||
      !bluetooth_medium_.StartAcceptingConnections(
          service_id, {.accepted_cb = [this, client, local_endpoint_info](
                                          BluetoothSocket socket) {
            if (!socket.IsValid()) {
              NEARBY_LOG(ERROR, "Invalid socket in accept callback: name=%s",
                         std::string(local_endpoint_info).c_str());
              return;
            }
            RunOnPcpHandlerThread([this, client, local_endpoint_info,
                                   socket = std::move(socket)]() mutable {
              std::string remote_device_name =
                  socket.GetRemoteDevice().GetName();
              auto channel = absl::make_unique<BluetoothEndpointChannel>(
                  remote_device_name, socket);
              ByteArray remote_device_info{remote_device_name};

              OnIncomingConnection(client, remote_device_info,
                                   std::move(channel),
                                   proto::connections::Medium::BLUETOOTH);
            });
          }})) {
    NEARBY_LOG(INFO, "BT failed to start accepting connections for service=%s",
               service_id.c_str());
    return proto::connections::UNKNOWN_MEDIUM;
  }

  NEARBY_LOG(INFO,
             "P2pClusterPcpHandler::StartBluetoothAdvertising: service=%s: "
             "make name; id=%s, hash=%s, name=%s",
             service_id.c_str(), local_endpoint_id.c_str(),
             absl::BytesToHexString(service_id_hash.data()).c_str(),
             absl::BytesToHexString(local_endpoint_info.data()).c_str());
  // Generate a BluetoothDeviceName with which to become Bluetooth discoverable.
  // TODO(b/169550050): Implement UWBAddress.
  std::string device_name(BluetoothDeviceName(
      kBluetoothDeviceNameVersion, GetPcp(), local_endpoint_id, service_id_hash,
      local_endpoint_info, ByteArray{}, web_rtc_state));
  if (device_name.empty()) {
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::StartBluetoothAdvertising: generate "
               "BluetoothDeviceName failed");
    bluetooth_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  } else {
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::StartBluetoothAdvertising: generate "
               "BluetoothDeviceName succeeded; device_name=%s",
               device_name.c_str());
  }

  NEARBY_LOG(
      INFO,
      "P2pClusterPcpHandler::StartBluetoothAdvertising: service=%s: come up",
      service_id.c_str());
  // Become Bluetooth discoverable.
  if (!bluetooth_medium_.TurnOnDiscoverability(device_name)) {
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::StartBluetoothAdvertising: failed to "
               "turn on discoverability, device_name=%s",
               device_name.c_str());
    bluetooth_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  } else {
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::StartBluetoothAdvertising: succeeded to "
               "turn on discoverability, device_name=%s",
               device_name.c_str());
  }
  NEARBY_LOG(
      INFO, "P2pClusterPcpHandler::StartBluetoothAdvertising: service=%s: done",
      service_id.c_str());
  return proto::connections::BLUETOOTH;
}

proto::connections::Medium P2pClusterPcpHandler::StartBluetoothDiscovery(
    BluetoothDiscoveredDeviceCallback callback, ClientProxy* client,
    const std::string& service_id) {
  if (bluetooth_radio_.Enable() &&
      bluetooth_medium_.StartDiscovery(std::move(callback))) {
    NEARBY_LOG(INFO, "P2pClusterPcpHandler::StartBluetoothDiscovery: ok");
    return proto::connections::BLUETOOTH;
  } else {
    NEARBY_LOG(INFO, "P2pClusterPcpHandler::StartBluetoothDiscovery: failed");
    return proto::connections::UNKNOWN_MEDIUM;
  }
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::BluetoothConnectImpl(
    ClientProxy* client, BluetoothEndpoint* endpoint) {
  BluetoothDevice& device = endpoint->bluetooth_device;

  BluetoothSocket bluetooth_socket =
      bluetooth_medium_.Connect(device, endpoint->service_id);
  if (!bluetooth_socket.IsValid()) {
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kBluetoothError},
    };
  }

  auto channel = absl::make_unique<BluetoothEndpointChannel>(
      endpoint->endpoint_id, bluetooth_socket);

  return BasePcpHandler::ConnectImplResult{
      .medium = proto::connections::Medium::BLUETOOTH,
      .status = {Status::kSuccess},
      .endpoint_channel = std::move(channel),
  };
}

proto::connections::Medium P2pClusterPcpHandler::StartBleAdvertising(
    ClientProxy* client, const std::string& service_id,
    const std::string& local_endpoint_id, const ByteArray& local_endpoint_info,
    const ConnectionOptions& options, WebRtcState web_rtc_state) {
  bool fast_advertisement = !options.fast_advertisement_service_uuid.empty();
  PowerLevel power_level =
      options.low_power ? PowerLevel::kLowPower : PowerLevel::kHighPower;

  // Start listening for connections before advertising in case a connection
  // request comes in very quickly. BLE allows connecting over BLE itself, as
  // well as advertising the Bluetooth MAC address to allow connecting over
  // Bluetooth Classic.
  NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::StartBleAdvertising: service_id="
                    << service_id << ": start";
  if (!ble_medium_.IsAcceptingConnections(service_id)) {
    if (!bluetooth_radio_.Enable() ||
        !ble_medium_.StartAcceptingConnections(
            service_id, {.accepted_cb = [this, client, local_endpoint_info](
                                            BleSocket socket,
                                            const std::string& service_id) {
              if (!socket.IsValid()) {
                NEARBY_LOG(INFO, "Invalid socket in accept callback: name=%s",
                           std::string(local_endpoint_info).c_str());
                return;
              }
              RunOnPcpHandlerThread([this, client, local_endpoint_info,
                                     service_id,
                                     socket = std::move(socket)]() mutable {
                std::string remote_peripheral_name =
                    socket.GetRemotePeripheral().GetName();
                auto channel = absl::make_unique<BleEndpointChannel>(
                    remote_peripheral_name, socket);
                ByteArray remote_peripheral_info =
                    socket.GetRemotePeripheral().GetAdvertisementBytes(
                        service_id);

                OnIncomingConnection(client, remote_peripheral_info,
                                     std::move(channel),
                                     proto::connections::Medium::BLE);
              });
            }})) {
      NEARBY_LOGS(INFO)
          << "Ble failed to start accepting connections for service_id="
          << service_id;
      return proto::connections::UNKNOWN_MEDIUM;
    }
    NEARBY_LOGS(INFO)
        << "Ble succeed to start accepting connections for service_id="
        << service_id;
  }

  if (ShouldAdvertiseBluetoothMacOverBle(power_level) ||
      ShouldAcceptBluetoothConnections(options)) {
    if (bluetooth_medium_.IsAvailable() &&
        !bluetooth_medium_.IsAcceptingConnections(service_id)) {
      if (!bluetooth_radio_.Enable() ||
          !bluetooth_medium_.StartAcceptingConnections(
              service_id, {.accepted_cb = [this, client, local_endpoint_info](
                                              BluetoothSocket socket) {
                if (!socket.IsValid()) {
                  NEARBY_LOG(INFO, "Invalid socket in accept callback: name=%s",
                             std::string(local_endpoint_info).c_str());
                  return;
                }
                RunOnPcpHandlerThread([this, client, local_endpoint_info,
                                       socket = std::move(socket)]() mutable {
                  std::string remote_device_name =
                      socket.GetRemoteDevice().GetName();
                  auto channel = absl::make_unique<BluetoothEndpointChannel>(
                      remote_device_name, socket);
                  ByteArray remote_device_info{remote_device_name};

                  OnIncomingConnection(client, remote_device_info,
                                       std::move(channel),
                                       proto::connections::Medium::BLUETOOTH);
                });
              }})) {
        NEARBY_LOGS(INFO)
            << "BT failed to start accepting connections for service_id="
            << service_id;
        ble_medium_.StopAcceptingConnections(service_id);
        return proto::connections::UNKNOWN_MEDIUM;
      }
      NEARBY_LOGS(INFO)
          << "BT succeed to start accepting connections for service_id="
          << service_id;
    }
  }

  NEARBY_LOG(INFO,
             "P2pClusterPcpHandler::StartBleAdvertising: service=%s: "
             "make advertisement; id=%s, name=%s",
             service_id.c_str(), local_endpoint_id.c_str(),
             std::string(local_endpoint_info).c_str());
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
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::StartBleAdvertising: generate "
               "BleAdvertisement failed");
    ble_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  } else {
    NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::StartBleAdvertising: generate "
                         "BleAdvertisement succeeded; advertisement_bytes="
                      << advertisement_bytes.data();
  }

  NEARBY_LOG(
      INFO, "P2pClusterPcpHandler::StartBleAdvertising: service_id=%s: come up",
      service_id.c_str());

  if (!ble_medium_.StartAdvertising(service_id, advertisement_bytes,
                                    options.fast_advertisement_service_uuid)) {
    NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::StartBleAdvertising: failed to "
                         "start advertising, advertisement_bytes=%p"
                      << advertisement_bytes.data();
    ble_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }
  NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::StartBleAdvertising: service_id="
                    << service_id << ": done";
  return proto::connections::BLE;
}

proto::connections::Medium P2pClusterPcpHandler::StartBleScanning(
    BleDiscoveredPeripheralCallback callback, ClientProxy* client,
    const std::string& service_id,
    const std::string& fast_advertisement_service_uuid) {
  if (bluetooth_radio_.Enable() &&
      ble_medium_.StartScanning(service_id, fast_advertisement_service_uuid,
                                std::move(callback))) {
    NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::StartBleScanning: ok";
    return proto::connections::BLE;
  } else {
    NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::StartBleScanning: failed";
    return proto::connections::UNKNOWN_MEDIUM;
  }
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::BleConnectImpl(
    ClientProxy* client, BleEndpoint* endpoint) {
  BlePeripheral& peripheral = endpoint->ble_peripheral;

  BleSocket ble_socket = ble_medium_.Connect(peripheral, endpoint->service_id);
  if (!ble_socket.IsValid()) {
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kBleError},
    };
  }

  auto channel =
      absl::make_unique<BleEndpointChannel>(endpoint->endpoint_id, ble_socket);

  return BasePcpHandler::ConnectImplResult{
      .medium = proto::connections::Medium::BLE,
      .status = {Status::kSuccess},
      .endpoint_channel = std::move(channel),
  };
}

proto::connections::Medium P2pClusterPcpHandler::StartWifiLanAdvertising(
    ClientProxy* client, const std::string& service_id,
    const ByteArray& service_id_hash, const std::string& local_endpoint_id,
    const ByteArray& local_endpoint_info, WebRtcState web_rtc_state) {
  // Start listening for connections before advertising in case a connection
  // request comes in very quickly.
  NEARBY_LOG(INFO,
             "P2pClusterPcpHandler::StartWifiLanAdvertising: service=%s: start",
             service_id.c_str());
  if (wifi_lan_medium_.IsAcceptingConnections(service_id)) {
    NEARBY_LOG(INFO, "WifiLan is already accepting connections for service=%s",
               service_id.c_str());
    return proto::connections::UNKNOWN_MEDIUM;
  }

  NEARBY_LOG(
      INFO,
      "P2pClusterPcpHandler::StartWifiLanAdvertising: service=%s: invoking",
      service_id.c_str());
  if (!wifi_lan_medium_.StartAcceptingConnections(
          service_id, {.accepted_cb = [this, client, local_endpoint_info](
                                          WifiLanSocket socket,
                                          const std::string& service_id) {
            if (!socket.IsValid()) {
              NEARBY_LOG(INFO, "Invalid socket in accept callback: name=%s",
                         std::string(local_endpoint_info).c_str());
              return;
            }
            RunOnPcpHandlerThread([this, client, local_endpoint_info,
                                   socket = std::move(socket)]() mutable {
              std::string remote_service_info_name =
                  socket.GetRemoteWifiLanService()
                      .GetServiceInfo()
                      .GetServiceInfoName();
              auto channel = absl::make_unique<WifiLanEndpointChannel>(
                  remote_service_info_name, socket);
              ByteArray remote_service_info{remote_service_info_name};

              OnIncomingConnection(client, remote_service_info,
                                   std::move(channel),
                                   proto::connections::Medium::WIFI_LAN);
            });
          }})) {
    NEARBY_LOG(INFO,
               "WifiLan failed to start accepting connections for service=%s",
               service_id.c_str());
    return proto::connections::UNKNOWN_MEDIUM;
  }

  NEARBY_LOG(INFO,
             "P2pClusterPcpHandler::StartWifiLanAdvertising: service=%s: "
             "make name; id=%s, hash=%s, endpoint info=%s",
             service_id.c_str(), local_endpoint_id.c_str(),
             absl::BytesToHexString(service_id_hash.data()).c_str(),
             absl::BytesToHexString(local_endpoint_info.data()).c_str());
  // Generate a WifiLanServiceInfo with which to become WifiLan discoverable.
  // TODO(b/169550050): Implement UWBAddress.
  WifiLanServiceInfo service_info{kWifiLanServiceInfoVersion,
                                  GetPcp(),
                                  local_endpoint_id,
                                  service_id_hash,
                                  local_endpoint_info,
                                  ByteArray{},
                                  web_rtc_state};
  NsdServiceInfo nsd_service_info{service_info};
  if (!nsd_service_info.IsValid()) {
    NEARBY_LOGS(INFO)
        << "P2pClusterPcpHandler::StartWifiLanAdvertising: generate "
           "NsdServiceInfo failed";
    wifi_lan_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  } else {
    NEARBY_LOGS(INFO)
        << "P2pClusterPcpHandler::StartWifiLanAdvertising: generate "
           "NsdServiceInfo succeeded; service_info_name="
        << nsd_service_info.GetServiceInfoName();
  }

  NEARBY_LOG(
      INFO,
      "P2pClusterPcpHandler::StartWifiLanAdvertising: service=%s: come up",
      service_id.c_str());

  if (!wifi_lan_medium_.StartAdvertising(service_id, nsd_service_info)) {
    NEARBY_LOG(INFO,
               "P2pClusterPcpHandler::StartWifiLanAdvertising: failed to "
               "start advertising, service_info_name=%s",
               nsd_service_info.GetServiceInfoName().c_str());
    wifi_lan_medium_.StopAcceptingConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }
  NEARBY_LOG(INFO,
             "P2pClusterPcpHandler::StartWifiLanAdvertising: service=%s: done",
             service_id.c_str());
  return proto::connections::WIFI_LAN;
}

proto::connections::Medium P2pClusterPcpHandler::StartWifiLanDiscovery(
    WifiLanDiscoveredServiceCallback callback, ClientProxy* client,
    const std::string& service_id) {
  if (wifi_lan_medium_.StartDiscovery(service_id, std::move(callback))) {
    NEARBY_LOG(INFO, "P2pClusterPcpHandler::StartWifiLanDiscovery: ok");
    return proto::connections::WIFI_LAN;
  } else {
    NEARBY_LOG(INFO, "P2pClusterPcpHandler::StartWifiLanDiscovery: failed");
    return proto::connections::UNKNOWN_MEDIUM;
  }
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::WifiLanConnectImpl(
    ClientProxy* client, WifiLanEndpoint* endpoint) {
  WifiLanService& wifi_lan_service = endpoint->wifi_lan_service;

  WifiLanSocket wifi_lan_socket =
      wifi_lan_medium_.Connect(wifi_lan_service, endpoint->service_id);
  if (!wifi_lan_socket.IsValid()) {
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kWifiLanError},
    };
  }

  auto channel = absl::make_unique<WifiLanEndpointChannel>(
      endpoint->endpoint_id, wifi_lan_socket);

  return BasePcpHandler::ConnectImplResult{
      .medium = proto::connections::Medium::WIFI_LAN,
      .status = {Status::kSuccess},
      .endpoint_channel = std::move(channel),
  };
}

proto::connections::Medium
P2pClusterPcpHandler::StartListeningForWebRtcConnections(
    ClientProxy* client, const std::string& service_id,
    const std::string& local_endpoint_id,
    const ByteArray& local_endpoint_info) {
  if (!webrtc_medium_.IsAvailable()) {
    return proto::connections::UNKNOWN_MEDIUM;
  }

  if (!webrtc_medium_.IsAcceptingConnections(service_id)) {
    mediums::PeerId self_id = CreatePeerIdFromAdvertisement(
        service_id, local_endpoint_id, local_endpoint_info);
    std::string empty_country_code;
    if (!webrtc_medium_.StartAcceptingConnections(
            service_id, self_id, Utils::BuildLocationHint(empty_country_code),
            {[this, client,
              local_endpoint_info](mediums::WebRtcSocketWrapper socket) {
              if (!socket.IsValid()) {
                NEARBY_LOG(INFO, "Invalid socket in accept callback: name=%s",
                           std::string(local_endpoint_info).c_str());
                return;
              }

              RunOnPcpHandlerThread(
                  [this, client, socket = std::move(socket)]() {
                    std::string remote_device_name = "WebRtcSocket";
                    auto channel = absl::make_unique<WebRtcEndpointChannel>(
                        remote_device_name, socket);
                    ByteArray remote_device_info{remote_device_name};

                    OnIncomingConnection(client, remote_device_info,
                                         std::move(channel),
                                         proto::connections::WEB_RTC);
                  });
            }})) {
      return proto::connections::UNKNOWN_MEDIUM;
    }
  }

  return proto::connections::WEB_RTC;
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::WebRtcConnectImpl(
    ClientProxy* client, WebRtcEndpoint* webrtc_endpoint) {
  std::string empty_country_code;
  mediums::WebRtcSocketWrapper socket_wrapper = webrtc_medium_.Connect(
      webrtc_endpoint->peer_id, Utils::BuildLocationHint(empty_country_code));
  if (!socket_wrapper.IsValid()) {
    return BasePcpHandler::ConnectImplResult{.status = {Status::kError}};
  }

  auto channel = absl::make_unique<WebRtcEndpointChannel>(
      webrtc_endpoint->endpoint_id, socket_wrapper);

  if (!channel) {
    socket_wrapper.Close();
    return BasePcpHandler::ConnectImplResult{.status = {Status::kError}};
  }

  return BasePcpHandler::ConnectImplResult{
      .medium = proto::connections::Medium::WEB_RTC,
      .status = {Status::kSuccess},
      .endpoint_channel = std::move(channel)};
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
