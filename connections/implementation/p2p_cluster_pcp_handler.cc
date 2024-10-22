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

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "connections/advertising_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/base_pcp_handler.h"
#include "connections/implementation/ble_advertisement.h"
#include "connections/implementation/ble_endpoint_channel.h"
#include "connections/implementation/ble_v2_endpoint_channel.h"
#include "connections/implementation/bluetooth_device_name.h"
#include "connections/implementation/bluetooth_endpoint_channel.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/injected_bluetooth_device_store.h"
#include "connections/implementation/mediums/bluetooth_classic.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/pcp.h"
#include "connections/implementation/pcp_handler.h"
#include "connections/implementation/wifi_lan_endpoint_channel.h"
#include "connections/implementation/wifi_lan_service_info.h"
#include "connections/medium_selector.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/power_level.h"
#include "connections/status.h"
#include "connections/v3/connection_listening_options.h"
#include "internal/flags/nearby_flags.h"
#include "internal/interop/device.h"
#include "internal/platform/ble.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/logging.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/os_name.h"
#include "internal/platform/types.h"
#include "internal/platform/wifi_lan.h"
#include "proto/connections_enums.pb.h"

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
      ble_v2_medium_(mediums->GetBleV2()),
      wifi_lan_medium_(mediums->GetWifiLan()),
      wifi_hotspot_medium_(mediums->GetWifiHotspot()),
      wifi_direct_medium_(mediums->GetWifiDirect()),
      webrtc_medium_(mediums->GetWebRtc()),
      injected_bluetooth_device_store_(injected_bluetooth_device_store) {}

P2pClusterPcpHandler::~P2pClusterPcpHandler() {
  NEARBY_VLOG(1) << __func__;
  Shutdown();
}

// Returns a vector or mediums sorted in order or decreasing priority for
// all the supported mediums.
// Example: WiFi_LAN, WEB_RTC, BT, BLE
std::vector<Medium> P2pClusterPcpHandler::GetConnectionMediumsByPriority() {
  std::vector<Medium> mediums;
  if (wifi_lan_medium_.IsAvailable()) {
    mediums.push_back(location::nearby::proto::connections::WIFI_LAN);
  }
  if (webrtc_medium_.IsAvailable()) {
    mediums.push_back(location::nearby::proto::connections::WEB_RTC);
  }
  if (bluetooth_medium_.IsAvailable()) {
    mediums.push_back(location::nearby::proto::connections::BLUETOOTH);
  }
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
    if (ble_v2_medium_.IsAvailable()) {
      mediums.push_back(location::nearby::proto::connections::BLE);
    }
  } else {
    if (ble_medium_.IsAvailable()) {
      mediums.push_back(location::nearby::proto::connections::BLE);
    }
  }

  return mediums;
}

Medium P2pClusterPcpHandler::GetDefaultUpgradeMedium() {
  return location::nearby::proto::connections::WIFI_LAN;
}

BasePcpHandler::StartOperationResult P2pClusterPcpHandler::StartAdvertisingImpl(
    ClientProxy* client, const std::string& service_id,
    const std::string& local_endpoint_id, const ByteArray& local_endpoint_info,
    const AdvertisingOptions& advertising_options) {
  std::vector<Medium> mediums_started_successfully;

  WebRtcState web_rtc_state{WebRtcState::kUnconnectable};

  if (advertising_options.allowed.wifi_lan) {
    Medium wifi_lan_medium =
        StartWifiLanAdvertising(client, service_id, local_endpoint_id,
                                local_endpoint_info, web_rtc_state);
    if (wifi_lan_medium !=
        location::nearby::proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOGS(INFO)
          << "P2pClusterPcpHandler::StartAdvertisingImpl: WifiLan added";
      mediums_started_successfully.push_back(wifi_lan_medium);
    }
  }

  if (advertising_options.allowed.bluetooth) {
    const ByteArray bluetooth_hash =
        GenerateHash(service_id, BluetoothDeviceName::kServiceIdHashLength);
    Medium bluetooth_medium = StartBluetoothAdvertising(
        client, service_id, bluetooth_hash, local_endpoint_id,
        local_endpoint_info, web_rtc_state);
    if (bluetooth_medium !=
        location::nearby::proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOGS(INFO)
          << "P2pClusterPcpHandler::StartAdvertisingImpl: BT started";

      // TODO(hais): update this after ble_v2 refactor.
      if (api::ImplementationPlatform::GetCurrentOS() ==
              api::OSName::kChromeOS &&
          !NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_connections_feature::
                  kEnableBleV2)) {
        if (ble_medium_.StartLegacyAdvertising(
                service_id, local_endpoint_id,
                advertising_options.fast_advertisement_service_uuid)) {
          NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::StartAdvertisingImpl: "
                               "Ble legacy started advertising";
          NEARBY_LOGS(INFO)
              << "P2pClusterPcpHandler::StartAdvertisingImpl: BT added";
          mediums_started_successfully.push_back(bluetooth_medium);
          bluetooth_classic_advertiser_client_id_ = client->GetClientId();
        } else {
          // TODO(hais): update this after ble_v2 refactor.
          NEARBY_LOGS(WARNING) << "P2pClusterPcpHandler::StartAdvertisingImpl: "
                                  "BLE legacy failed, revert BTC";
          bluetooth_medium_.TurnOffDiscoverability();
          bluetooth_medium_.StopAcceptingConnections(service_id);
        }
      } else if ((api::ImplementationPlatform::GetCurrentOS() ==
                      api::OSName::kChromeOS ||
                  api::ImplementationPlatform::GetCurrentOS() ==
                      api::OSName::kLinux) &&
                 NearbyFlags::GetInstance().GetBoolFlag(
                     config_package_nearby::nearby_connections_feature::
                         kEnableBleV2)) {
        if (ble_v2_medium_.StartLegacyAdvertising(
                service_id, local_endpoint_id,
                advertising_options.fast_advertisement_service_uuid)) {
          NEARBY_LOGS(INFO)
              << __func__ << "Ble v2 started advertising for legacy device.";
          mediums_started_successfully.push_back(bluetooth_medium);
          NEARBY_LOGS(INFO) << __func__ << "After Ble v2, BT added";
          bluetooth_classic_advertiser_client_id_ = client->GetClientId();
        } else {
          NEARBY_LOGS(WARNING) << "P2pClusterPcpHandler::StartAdvertisingImpl: "
                                  "BLE legacy failed, revert BTC";
          bluetooth_medium_.TurnOffDiscoverability();
          bluetooth_medium_.StopAcceptingConnections(service_id);
        }
      } else {
        NEARBY_LOGS(INFO)
            << "P2pClusterPcpHandler::StartAdvertisingImpl: BT added";
        mediums_started_successfully.push_back(bluetooth_medium);
        bluetooth_classic_advertiser_client_id_ = client->GetClientId();
      }
    }
  }

  if (advertising_options.allowed.ble) {
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
      Medium ble_v2_medium = StartBleV2Advertising(
          client, service_id, local_endpoint_id, local_endpoint_info,
          advertising_options, web_rtc_state);
      if (ble_v2_medium !=
          location::nearby::proto::connections::UNKNOWN_MEDIUM) {
        NEARBY_LOGS(INFO)
            << "P2pClusterPcpHandler::StartAdvertisingImpl: Ble added";
        mediums_started_successfully.push_back(ble_v2_medium);
      }
    } else {
      Medium ble_medium = StartBleAdvertising(
          client, service_id, local_endpoint_id, local_endpoint_info,
          advertising_options, web_rtc_state);
      if (ble_medium != location::nearby::proto::connections::UNKNOWN_MEDIUM) {
        NEARBY_LOGS(INFO)
            << "P2pClusterPcpHandler::StartAdvertisingImpl: Ble added";
        mediums_started_successfully.push_back(ble_medium);
      }
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
    // TODO(hais): update this after ble_v2 refactor.
    if (api::ImplementationPlatform::GetCurrentOS() == api::OSName::kChromeOS &&
        !NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
      ble_medium_.StopLegacyAdvertising(client->GetAdvertisingServiceId());
    } else if ((api::ImplementationPlatform::GetCurrentOS() ==
                    api::OSName::kChromeOS ||
                api::ImplementationPlatform::GetCurrentOS() ==
                    api::OSName::kLinux) &&
               NearbyFlags::GetInstance().GetBoolFlag(
                   config_package_nearby::nearby_connections_feature::
                       kEnableBleV2)) {
      ble_v2_medium_.StopLegacyAdvertising(client->GetAdvertisingServiceId());
    }
    bluetooth_classic_advertiser_client_id_ = 0;
  } else {
    NEARBY_LOGS(INFO) << "Skipped BT TurnOffDiscoverability for client="
                      << client->GetClientId()
                      << ", client that turned on discoverability is "
                      << bluetooth_classic_advertiser_client_id_;
  }

  bluetooth_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());

  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
    ble_v2_medium_.StopAdvertising(client->GetAdvertisingServiceId());
    ble_v2_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());
  } else {
    ble_medium_.StopAdvertising(client->GetAdvertisingServiceId());
    ble_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());
  }

  wifi_lan_medium_.StopAdvertising(client->GetAdvertisingServiceId());
  wifi_lan_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());

  return {Status::kSuccess};
}

bool P2pClusterPcpHandler::IsRecognizedBluetoothEndpoint(
    const std::string& name_string, const std::string& service_id,
    const BluetoothDeviceName& name) const {
  if (!name.IsValid()) {
    NEARBY_LOGS(INFO) << name_string
                      << " doesn't have any endpoint id, discarding.";
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
                client, std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
                            {device_name.GetEndpointId(),
                             device_name.GetEndpointInfo(), service_id,
                             Medium::BLUETOOTH, device_name.GetWebRtcState()},
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

        // By this point, the BluetoothDevice passed to us has a different
        // name than what we may have discovered before. We need to iterate
        // over the found BluetoothEndpoints and compare their addresses to
        // see the devices are the same. We are not guaranteed to discover a
        // match, since the old name may not have been formatted for Nearby
        // Connections.
        for (auto endpoint : GetDiscoveredEndpoints(Medium::BLUETOOTH)) {
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
                 service_id, Medium::BLUETOOTH, device_name.GetWebRtcState()},
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
        OnEndpointLost(client, DiscoveredEndpoint{device_name.GetEndpointId(),
                                                  device_name.GetEndpointInfo(),
                                                  service_id, Medium::BLUETOOTH,
                                                  WebRtcState::kUndefined});
      });
}

bool P2pClusterPcpHandler::IsRecognizedBleEndpoint(
    const std::string& service_id,
    const BleAdvertisement& advertisement) const {
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
        if (!client->IsDiscovering() || stop_.Get()) {
          NEARBY_LOGS(WARNING)
              << "Skipping discovery of BleAdvertisement header "
              << absl::BytesToHexString(advertisement_bytes.data())
              << " because we are no longer discovering.";
          return;
        }

        auto ble_status_or = BleAdvertisement::CreateBleAdvertisement(
            fast_advertisement, advertisement_bytes);
        if (!ble_status_or.ok()) {
          NEARBY_LOGS(ERROR) << ble_status_or.status().ToString();
          return;
        }
        const auto& advertisement = ble_status_or.value();

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

        StopEndpointLostByMediumAlarm(advertisement.GetEndpointId(),
                                      Medium::BLE);

        // Report the discovered endpoint to the client.
        NEARBY_LOGS(INFO) << "Found BleAdvertisement "
                          << absl::BytesToHexString(advertisement_bytes.data())
                          << " (with endpoint_id="
                          << advertisement.GetEndpointId()
                          << ", and endpoint_info="
                          << absl::BytesToHexString(
                                 advertisement.GetEndpointInfo().data())
                          << ").";
        OnEndpointFound(
            client,
            std::make_shared<BleEndpoint>(BleEndpoint{
                {advertisement.GetEndpointId(), advertisement.GetEndpointInfo(),
                 service_id, Medium::BLE, advertisement.GetWebRtcState()},
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

        StopEndpointLostByMediumAlarm(advertisement.GetEndpointId(),
                                      Medium::BLUETOOTH);
        OnEndpointFound(client,
                        std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
                            {
                                advertisement.GetEndpointId(),
                                advertisement.GetEndpointInfo(),
                                service_id,
                                Medium::BLUETOOTH,
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
  NEARBY_LOGS(INFO) << "Ble: [LOST, SCHED] peripheral_name=" << peripheral_name;
  RunOnPcpHandlerThread(
      "p2p-ble-device-lost",
      [this, client, service_id, &peripheral]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering() || stop_.Get()) {
          NEARBY_LOGS(WARNING) << "Ignoring lost BlePeripheral  because we are "
                                  "no longer discovering.";
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
                                     Medium::BLE,
                                     WebRtcState::kUndefined,
                                 });
        }
      });
}

bool P2pClusterPcpHandler::IsRecognizedBleV2Endpoint(
    absl::string_view service_id, const BleAdvertisement& advertisement) const {
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
    ByteArray expected_service_id_hash = GenerateHash(
        std::string(service_id), BleAdvertisement::kServiceIdHashLength);

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

void P2pClusterPcpHandler::BleV2PeripheralDiscoveredHandler(
    ClientProxy* client, BleV2Peripheral peripheral,
    const std::string& service_id, const ByteArray& advertisement_bytes,
    bool fast_advertisement) {
  // TODO(edwinwu): Move the lambda to a named function.
  RunOnPcpHandlerThread(
      "p2p-ble-peripheral-discovered",
      [this, client, peripheral = std::move(peripheral), service_id,
       advertisement_bytes, fast_advertisement]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering() || stop_.Get()) {
          NEARBY_LOGS(WARNING)
              << "Skipping discovery of BleAdvertisement header "
              << absl::BytesToHexString(advertisement_bytes.data())
              << " because we are no longer discovering.";
          return;
        }

        if (client->GetDiscoveryOptions()
                .fast_advertisement_service_uuid.empty() &&
            fast_advertisement) {
          NEARBY_LOGS(INFO) << "Ignore the fast advertisement due to cient "
                               "doesn't receive it.";
          return;
        }

        auto ble_status_or = BleAdvertisement::CreateBleAdvertisement(
            fast_advertisement, advertisement_bytes);
        if (!ble_status_or.ok()) {
          NEARBY_LOGS(ERROR) << ble_status_or.status();
          return;
        }
        const auto& advertisement = ble_status_or.value();

        // Make sure the BLE advertisement points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedBleV2Endpoint(service_id, advertisement)) return;

        // Report the discovered endpoint to the client.
        BleV2EndpointState ble_endpoint_state;
        ByteArray peripheral_id = peripheral.GetId();
        found_endpoints_in_ble_discover_cb_.insert(
            {peripheral_id, ble_endpoint_state});

        ble_endpoint_state.ble = true;
        found_endpoints_in_ble_discover_cb_[peripheral_id] = ble_endpoint_state;
        NEARBY_LOGS(INFO) << "Found BleAdvertisement "
                          << absl::BytesToHexString(advertisement_bytes.data())
                          << " (with endpoint_id="
                          << advertisement.GetEndpointId()
                          << ", and endpoint_info="
                          << absl::BytesToHexString(
                                 advertisement.GetEndpointInfo().data())
                          << ").";
        StopEndpointLostByMediumAlarm(advertisement.GetEndpointId(),
                                      Medium::BLE);
        OnEndpointFound(
            client,
            std::make_shared<BleV2Endpoint>(BleV2Endpoint{
                {advertisement.GetEndpointId(), advertisement.GetEndpointInfo(),
                 service_id, Medium::BLE, advertisement.GetWebRtcState()},
                std::move(peripheral),
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

        ble_endpoint_state.bt = true;
        found_endpoints_in_ble_discover_cb_[peripheral_id] = ble_endpoint_state;
        StopEndpointLostByMediumAlarm(advertisement.GetEndpointId(),
                                      Medium::BLUETOOTH);

        OnEndpointFound(client,
                        std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
                            {
                                advertisement.GetEndpointId(),
                                advertisement.GetEndpointInfo(),
                                service_id,
                                Medium::BLUETOOTH,
                                advertisement.GetWebRtcState(),
                            },
                            remote_bluetooth_device,
                        }));
      });
}

// TODO(b/222392304): More test coverage.
void P2pClusterPcpHandler::BleV2PeripheralLostHandler(
    ClientProxy* client, BleV2Peripheral peripheral,
    const std::string& service_id, const ByteArray& advertisement_bytes,
    bool fast_advertisement) {
  RunOnPcpHandlerThread(
      "p2p-ble-peripheral-lost",
      [this, client, service_id, peripheral = std::move(peripheral),
       advertisement_bytes, fast_advertisement]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering() || stop_.Get()) {
          NEARBY_LOGS(WARNING)
              << "Ignoring lost BlePeripheral "
              << absl::BytesToHexString(peripheral.GetId().data())
              << " because we are no longer discovering.";
          return;
        }

        auto ble_status_or = BleAdvertisement::CreateBleAdvertisement(
            fast_advertisement, advertisement_bytes);
        if (!ble_status_or.ok()) {
          NEARBY_LOGS(ERROR) << ble_status_or.status();
          return;
        }
        const auto& advertisement = ble_status_or.value();

        // Make sure the BLE advertisement points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedBleV2Endpoint(service_id, advertisement)) return;

        // Remove this BlePeripheral from found_ble_endpoints_, and
        // report the endpoint as lost to the client.
        auto const item =
            found_endpoints_in_ble_discover_cb_.find(peripheral.GetId());
        if (item == found_endpoints_in_ble_discover_cb_.end()) {
          return;
        }
        BleV2EndpointState ble_endpoint_state(item->second);
        found_endpoints_in_ble_discover_cb_.erase(item);

        if (ble_endpoint_state.ble) {
          // Report the lost endpoint to the client.
          NEARBY_LOGS(INFO)
              << "Lost BleEndpoint for BlePeripheral "
              << absl::BytesToHexString(peripheral.GetId().data())
              << " (with endpoint_id=" << advertisement.GetEndpointId()
              << " and endpoint_info="
              << absl::BytesToHexString(advertisement.GetEndpointInfo().data())
              << ").";
          OnEndpointLost(client, DiscoveredEndpoint{
                                     advertisement.GetEndpointId(),
                                     advertisement.GetEndpointInfo(),
                                     service_id,
                                     Medium::BLE,
                                     WebRtcState::kUndefined,
                                 });
        }
        if (ble_endpoint_state.bt) {
          // Report the lost endpoint to the client.
          NEARBY_LOGS(INFO)
              << "Lost BluetoothEndpoint for BlePeripheral "
              << absl::BytesToHexString(peripheral.GetId().data())
              << " (with endpoint_id=" << advertisement.GetEndpointId()
              << " and endpoint_info="
              << absl::BytesToHexString(advertisement.GetEndpointInfo().data())
              << ").";
          OnEndpointLost(client, DiscoveredEndpoint{
                                     advertisement.GetEndpointId(),
                                     advertisement.GetEndpointInfo(),
                                     service_id,
                                     Medium::BLUETOOTH,
                                     WebRtcState::kUndefined,
                                 });
        }
      });
}

void P2pClusterPcpHandler::BleV2InstantLostHandler(
    ClientProxy* client, BleV2Peripheral peripheral,
    const std::string& service_id, const ByteArray& advertisement_bytes,
    bool fast_advertisement) {
  RunOnPcpHandlerThread(
      "p2p-ble-peripheral-instant-lost",
      [this, client, service_id, peripheral = std::move(peripheral),
       advertisement_bytes, fast_advertisement]() RUN_ON_PCP_HANDLER_THREAD() {
        std::string service_id = client->GetDiscoveryServiceId();

        if (!client->IsDiscovering() || stop_.Get()) {
          NEARBY_LOGS(WARNING)
              << "Ignoring instant lost BlePeripheral "
              << absl::BytesToHexString(peripheral.GetId().data())
              << " because we are no longer discovering.";
          return;
        }

        NEARBY_LOGS(INFO) << "Processing instant lost on BlePeripheral "
                          << absl::BytesToHexString(peripheral.GetId().data());
        auto ble_status_or = BleAdvertisement::CreateBleAdvertisement(
            fast_advertisement, advertisement_bytes);
        if (!ble_status_or.ok()) {
          NEARBY_LOGS(ERROR) << ble_status_or.status();
          return;
        }
        const auto& advertisement = ble_status_or.value();

        // Make sure the BLE advertisement points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedBleV2Endpoint(service_id, advertisement)) return;

        // Remove this BlePeripheral from found_ble_endpoints_, and
        // report the endpoint as lost to the client.
        auto const item =
            found_endpoints_in_ble_discover_cb_.find(peripheral.GetId());
        if (item == found_endpoints_in_ble_discover_cb_.end()) {
          return;
        }

        found_endpoints_in_ble_discover_cb_.erase(item);

        // Report the instant lost endpoint.
        OnInstantLost(client, advertisement.GetEndpointId(),
                      advertisement.GetEndpointInfo());
      });
}

void P2pClusterPcpHandler::BleV2LegacyDeviceDiscoveredHandler() {
  if (!NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kDisableBluetoothClassicScanning)) {
    return;
  }

  RunOnPcpHandlerThread(
      "p2p-ble-legacy-peripheral-discovered",
      [this]() RUN_ON_PCP_HANDLER_THREAD() {
        if (paused_bluetooth_clients_discoveries_.empty()) {
          return;
        }

        NEARBY_LOGS(INFO) << "Found nearby legacy BLE device, pending "
                             "bluetooth discovery size :"
                          << paused_bluetooth_clients_discoveries_.size();

        for (auto& paused_bluetooth_client :
             paused_bluetooth_clients_discoveries_) {
          if (!paused_bluetooth_client.second->IsDiscoveringServiceId(
                  paused_bluetooth_client.first)) {
            NEARBY_LOGS(INFO) << "Do not start bluetooth scanning since client "
                                 "is no longer discovering for service id: "
                              << paused_bluetooth_client.first;
            continue;
          }

          // Start the paused bluetooth discovery.
          StartBluetoothDiscovery(paused_bluetooth_client.second,
                                  paused_bluetooth_client.first);
        }

        // Remove all pending bluetooth clients.
        paused_bluetooth_clients_discoveries_.clear();
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
        StopEndpointLostByMediumAlarm(wifi_lan_service_info.GetEndpointId(),
                                      Medium::WIFI_LAN);
        OnEndpointFound(client,
                        std::make_shared<WifiLanEndpoint>(WifiLanEndpoint{
                            {
                                wifi_lan_service_info.GetEndpointId(),
                                wifi_lan_service_info.GetEndpointInfo(),
                                service_id,
                                Medium::WIFI_LAN,
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
                                   Medium::WIFI_LAN,
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

  std::vector<Medium> mediums_started_successfully;

  if (discovery_options.allowed.wifi_lan) {
    Medium wifi_lan_medium = StartWifiLanDiscovery(client, service_id);
    if (wifi_lan_medium !=
        location::nearby::proto::connections::UNKNOWN_MEDIUM) {
      NEARBY_LOGS(INFO)
          << "P2pClusterPcpHandler::StartDiscoveryImpl: WifiLan added";
      mediums_started_successfully.push_back(wifi_lan_medium);
    }
  }

  if (discovery_options.allowed.ble) {
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
      Medium ble_v2_medium =
          StartBleV2Scanning(client, service_id, discovery_options);
      if (ble_v2_medium !=
          location::nearby::proto::connections::UNKNOWN_MEDIUM) {
        NEARBY_LOGS(INFO)
            << "P2pClusterPcpHandler::StartDiscoveryImpl: Ble v2 added.";
        mediums_started_successfully.push_back(ble_v2_medium);
      }
    } else {
      Medium ble_medium =
          StartBleScanning(client, service_id,
                           discovery_options.fast_advertisement_service_uuid);
      if (ble_medium != location::nearby::proto::connections::UNKNOWN_MEDIUM) {
        NEARBY_LOGS(INFO)
            << "P2pClusterPcpHandler::StartDiscoveryImpl: Ble added.";
        mediums_started_successfully.push_back(ble_medium);
      }
    }
  }

  if (discovery_options.allowed.bluetooth) {
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kDisableBluetoothClassicScanning)) {
      StartBluetoothDiscoveryWithPause(client, service_id, discovery_options,
                                       mediums_started_successfully);
    } else {
      Medium bluetooth_medium = StartBluetoothDiscovery(client, service_id);
      if (bluetooth_medium !=
          location::nearby::proto::connections::UNKNOWN_MEDIUM) {
        NEARBY_LOGS(INFO)
            << "P2pClusterPcpHandler::StartDiscoveryImpl: BT added";
        mediums_started_successfully.push_back(bluetooth_medium);
        bluetooth_classic_client_id_to_service_id_map_.insert(
            {client->GetClientId(), service_id});
      }
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
  if (bluetooth_classic_client_id_to_service_id_map_.contains(
          client->GetClientId())) {
    bluetooth_medium_.StopDiscovery(
        bluetooth_classic_client_id_to_service_id_map_.at(
            client->GetClientId()));
    bluetooth_classic_client_id_to_service_id_map_.erase(client->GetClientId());
  } else {
    NEARBY_LOGS(INFO) << "Skipped BT StopDiscovery for client="
                      << client->GetClientId()
                      << " because it is not in discovery.";
  }

  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
    ble_v2_medium_.StopScanning(client->GetDiscoveryServiceId());
  } else {
    ble_medium_.StopScanning(client->GetDiscoveryServiceId());
  }

  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kDisableBluetoothClassicScanning)) {
    paused_bluetooth_clients_discoveries_.erase(
        client->GetDiscoveryServiceId());
  }

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
    NEARBY_LOGS(WARNING) << "InjectEndpointImpl: Invalid parameters.";
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
    case Medium::BLUETOOTH: {
      auto* bluetooth_endpoint = down_cast<BluetoothEndpoint*>(endpoint);
      if (bluetooth_endpoint) {
        return BluetoothConnectImpl(client, bluetooth_endpoint);
      }
      break;
    }
    case Medium::BLE: {
      if (NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_connections_feature::
                  kEnableBleV2)) {
        auto* ble_v2_endpoint = down_cast<BleV2Endpoint*>(endpoint);
        if (ble_v2_endpoint) {
          return BleV2ConnectImpl(client, ble_v2_endpoint);
        }

      } else {
        auto* ble_endpoint = down_cast<BleEndpoint*>(endpoint);
        if (ble_endpoint) {
          return BleConnectImpl(client, ble_endpoint);
        }
      }
      break;
    }
    case Medium::WIFI_LAN: {
      auto* wifi_lan_endpoint = down_cast<WifiLanEndpoint*>(endpoint);
      if (wifi_lan_endpoint) {
        return WifiLanConnectImpl(client, wifi_lan_endpoint);
      }
      break;
    }
    case Medium::WEB_RTC: {
      break;
    }
    default:
      break;
  }

  return BasePcpHandler::ConnectImplResult{
      .status = {Status::kError},
  };
}

BasePcpHandler::StartOperationResult
P2pClusterPcpHandler::StartListeningForIncomingConnectionsImpl(
    ClientProxy* client_proxy, absl::string_view service_id,
    absl::string_view local_endpoint_id,
    v3::ConnectionListeningOptions options) {
  std::vector<Medium> started_mediums;
  if (options.enable_bluetooth_listening &&
      !bluetooth_medium_.IsAcceptingConnections(std::string(service_id))) {
    if (!bluetooth_medium_.StartAcceptingConnections(
            std::string(service_id),
            absl::bind_front(
                &P2pClusterPcpHandler::BluetoothConnectionAcceptedHandler, this,
                client_proxy, local_endpoint_id,
                options.listening_endpoint_type))) {
      NEARBY_LOGS(WARNING)
          << "Failed to start listening for incoming connections on Bluetooth";
    } else {
      started_mediums.push_back(
          location::nearby::proto::connections::BLUETOOTH);
    }
  }
  // ble
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
    // ble_v2
    if (options.enable_ble_listening &&
        !ble_v2_medium_.IsAcceptingConnections(std::string(service_id))) {
      if (!ble_v2_medium_.StartAcceptingConnections(
              std::string(service_id),
              absl::bind_front(
                  &P2pClusterPcpHandler::BleV2ConnectionAcceptedHandler, this,
                  client_proxy, local_endpoint_id,
                  options.listening_endpoint_type))) {
        NEARBY_LOGS(WARNING)
            << "Failed to start listening for incoming connections on ble_v2";
      } else {
        started_mediums.push_back(location::nearby::proto::connections::BLE);
      }
    }
  } else {
    // ble v1
    if (options.enable_ble_listening &&
        !ble_medium_.IsAcceptingConnections(std::string(service_id))) {
      if (!ble_medium_.StartAcceptingConnections(
              std::string(service_id),
              absl::bind_front(
                  &P2pClusterPcpHandler::BleConnectionAcceptedHandler, this,
                  client_proxy, local_endpoint_id,
                  options.listening_endpoint_type))) {
        NEARBY_LOGS(WARNING)
            << "Failed to start listening for incoming connections on ble";
      } else {
        started_mediums.push_back(location::nearby::proto::connections::BLE);
      }
    }
  }
  if (options.enable_wlan_listening &&
      !wifi_lan_medium_.IsAcceptingConnections(std::string(service_id))) {
    if (!wifi_lan_medium_.StartAcceptingConnections(
            std::string(service_id),
            absl::bind_front(
                &P2pClusterPcpHandler::WifiLanConnectionAcceptedHandler, this,
                client_proxy, local_endpoint_id, "",
                options.listening_endpoint_type))) {
      NEARBY_LOGS(WARNING)
          << "Failed to start listening for incoming connections on wifi_lan";
    } else {
      started_mediums.push_back(location::nearby::proto::connections::WIFI_LAN);
    }
  }
  if (started_mediums.empty()) {
    NEARBY_LOGS(WARNING) << absl::StrFormat(
        "Failed StartListeningForIncomingConnectionsImpl() for client %d for "
        "service_id %s",
        client_proxy->GetClientId(), service_id);
    return StartOperationResult{
        .status = {Status::kError},
    };
  }
  return BasePcpHandler::StartOperationResult{
      .status = {Status::kSuccess}, .mediums = std::move(started_mediums)};
}

void P2pClusterPcpHandler::StopListeningForIncomingConnectionsImpl(
    ClientProxy* client) {
  if (wifi_lan_medium_.IsAcceptingConnections(
          client->GetListeningForIncomingConnectionsServiceId())) {
    if (!wifi_lan_medium_.StopAcceptingConnections(
            client->GetListeningForIncomingConnectionsServiceId())) {
      NEARBY_LOGS(WARNING)
          << "Unable to stop wifi lan from accepting connections.";
    }
  }
  if (bluetooth_medium_.IsAcceptingConnections(
          client->GetListeningForIncomingConnectionsServiceId())) {
    if (!bluetooth_medium_.StopAcceptingConnections(
            client->GetListeningForIncomingConnectionsServiceId())) {
      NEARBY_LOGS(WARNING)
          << "Unable to stop bluetooth medium from accepting connections.";
    }
  }
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
    if (ble_v2_medium_.IsAcceptingConnections(
            client->GetListeningForIncomingConnectionsServiceId())) {
      if (!ble_v2_medium_.StopAcceptingConnections(
              client->GetListeningForIncomingConnectionsServiceId())) {
        NEARBY_LOGS(WARNING)
            << "Unable to stop ble_v2 medium from accepting connections.";
      }
    }
  } else {
    if (ble_medium_.IsAcceptingConnections(
            client->GetListeningForIncomingConnectionsServiceId())) {
      if (!ble_medium_.StopAcceptingConnections(
              client->GetListeningForIncomingConnectionsServiceId())) {
        NEARBY_LOGS(WARNING)
            << "Unable to stop ble medium from accepting connections.";
      }
    }
  }
}

BasePcpHandler::StartOperationResult
P2pClusterPcpHandler::UpdateAdvertisingOptionsImpl(
    ClientProxy* client, absl::string_view service_id,
    absl::string_view local_endpoint_id, absl::string_view local_endpoint_info,
    const AdvertisingOptions& advertising_options) {
  AdvertisingOptions old_options = client->GetAdvertisingOptions();
  bool needs_restart = old_options.low_power != advertising_options.low_power;
  // ble
  if (NeedsToTurnOffAdvertisingMedium(Medium::BLE, old_options,
                                      advertising_options) ||
      needs_restart) {
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
      mediums_->GetBleV2().StopAdvertising(std::string(service_id));
      mediums_->GetBleV2().StopAcceptingConnections(std::string(service_id));
    } else {
      mediums_->GetBle().StopAdvertising(std::string(service_id));
      mediums_->GetBle().StopAcceptingConnections(std::string(service_id));
    }
  }
  // wifi lan
  if (NeedsToTurnOffAdvertisingMedium(Medium::WIFI_LAN, old_options,
                                      advertising_options) ||
      needs_restart) {
    mediums_->GetWifiLan().StopAdvertising(std::string(service_id));
    mediums_->GetWifiLan().StopAcceptingConnections(std::string(service_id));
  }
  // Bluetooth classic
  if (NeedsToTurnOffAdvertisingMedium(Medium::BLUETOOTH, old_options,
                                      advertising_options) ||
      needs_restart) {
    // BT classic equivalent for advertising.
    mediums_->GetBluetoothClassic().TurnOffDiscoverability();
    mediums_->GetBluetoothClassic().StopAcceptingConnections(
        std::string(service_id));
    // TODO(hais): update this after ble_v2 refactor.
    if (api::ImplementationPlatform::GetCurrentOS() == api::OSName::kChromeOS) {
      mediums_->GetBle().StopLegacyAdvertising(std::string(service_id));
    } else if ((api::ImplementationPlatform::GetCurrentOS() ==
                    api::OSName::kChromeOS ||
                api::ImplementationPlatform::GetCurrentOS() ==
                    api::OSName::kLinux) &&
               NearbyFlags::GetInstance().GetBoolFlag(
                   config_package_nearby::nearby_connections_feature::
                       kEnableBleV2)) {
      mediums_->GetBleV2().StopLegacyAdvertising(
          client->GetAdvertisingServiceId());
    }
  }

  // restart
  std::vector<Medium> restarted_mediums;
  Status status = {Status::kSuccess};
  WebRtcState web_rtc_state = webrtc_medium_.IsAvailable()
                                  ? WebRtcState::kConnectable
                                  : WebRtcState::kUndefined;
  // ble
  auto new_mediums = advertising_options.allowed;
  auto old_mediums = old_options.allowed;
  if (new_mediums.ble) {
    if (old_mediums.ble && !needs_restart) {
      restarted_mediums.push_back(Medium::BLE);
    } else {
      if (NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_connections_feature::
                  kEnableBleV2)) {
        if (StartBleV2Advertising(
                client, std::string(service_id), std::string(local_endpoint_id),
                ByteArray(std::string(local_endpoint_info)),
                advertising_options, web_rtc_state) != Medium::UNKNOWN_MEDIUM) {
          restarted_mediums.push_back(Medium::BLE);
        } else {
          status = {Status::kBleError};
        }
      } else {
        if (StartBleAdvertising(
                client, std::string(service_id), std::string(local_endpoint_id),
                ByteArray(std::string(local_endpoint_info)),
                advertising_options, web_rtc_state) != Medium::UNKNOWN_MEDIUM) {
          restarted_mediums.push_back(Medium::BLE);
        } else {
          status = {Status::kBleError};
        }
      }
    }
  }
  // wifi lan
  if (new_mediums.wifi_lan && !advertising_options.low_power) {
    if (old_mediums.wifi_lan && !needs_restart) {
      restarted_mediums.push_back(Medium::WIFI_LAN);
    } else {
      if (StartWifiLanAdvertising(client, std::string(service_id),
                                  std::string(local_endpoint_id),
                                  ByteArray(std::string(local_endpoint_info)),
                                  web_rtc_state) != Medium::UNKNOWN_MEDIUM) {
        restarted_mediums.push_back(Medium::WIFI_LAN);
      } else {
        status = {Status::kWifiLanError};
      }
    }
  }
  // bluetooth classic
  if (new_mediums.bluetooth && !advertising_options.low_power) {
    if (old_mediums.bluetooth && !needs_restart) {
      restarted_mediums.push_back(Medium::BLUETOOTH);
    } else {
      const ByteArray bluetooth_hash = GenerateHash(
          std::string(service_id), BluetoothDeviceName::kServiceIdHashLength);
      if (StartBluetoothAdvertising(client, std::string(service_id),
                                    bluetooth_hash,
                                    std::string(local_endpoint_id),
                                    ByteArray(std::string(local_endpoint_info)),
                                    web_rtc_state) != Medium::UNKNOWN_MEDIUM) {
        // TODO(hais): update this after ble_v2 refactor.
        if (api::ImplementationPlatform::GetCurrentOS() ==
                api::OSName::kChromeOS &&
            !NearbyFlags::GetInstance().GetBoolFlag(
                config_package_nearby::nearby_connections_feature::
                    kEnableBleV2)) {
          if (ble_medium_.StartLegacyAdvertising(
                  std::string(service_id), std::string(local_endpoint_id),
                  advertising_options.fast_advertisement_service_uuid)) {
            NEARBY_LOGS(INFO)
                << "P2pClusterPcpHandler::UpdateAdvertisingOptionsImpl: "
                   "Ble legacy started advertising";
            NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::"
                                 "UpdateAdvertisingOptionsImpl: BT added";
            restarted_mediums.push_back(Medium::BLUETOOTH);
          } else {
            NEARBY_LOGS(WARNING)
                << "P2pClusterPcpHandler::UpdateAdvertisingOptionsImpl: BLE "
                   "legacy failed, revert BTC";
            bluetooth_medium_.TurnOffDiscoverability();
            bluetooth_medium_.StopAcceptingConnections(std::string(service_id));
          }
        } else if ((api::ImplementationPlatform::GetCurrentOS() ==
                        api::OSName::kChromeOS ||
                    api::ImplementationPlatform::GetCurrentOS() ==
                        api::OSName::kLinux) &&
                   NearbyFlags::GetInstance().GetBoolFlag(
                       config_package_nearby::nearby_connections_feature::
                           kEnableBleV2)) {
          if (ble_v2_medium_.StartLegacyAdvertising(
                  std::string(service_id), std::string(local_endpoint_id),
                  advertising_options.fast_advertisement_service_uuid)) {
            NEARBY_LOGS(INFO)
                << __func__ << "Ble v2 started advertising for legacy device.";
            restarted_mediums.push_back(Medium::BLUETOOTH);
            NEARBY_LOGS(INFO) << __func__
                              << "After Ble v2 started advertising, for "
                                 "legacy, BT added to restarted mediums";
          } else {
            NEARBY_LOGS(WARNING)
                << __func__
                << "BLE v2 failed advertising for legacy device, revert BTC";
            bluetooth_medium_.TurnOffDiscoverability();
            bluetooth_medium_.StopAcceptingConnections(std::string(service_id));
          }

        } else {
          restarted_mediums.push_back(Medium::BLUETOOTH);
        }
      } else {
        return StartOperationResult{.status = {Status::kBluetoothError},
                                    .mediums = restarted_mediums};
      }
    }
  }
  return StartOperationResult{
      .status = status,
      .mediums = restarted_mediums,
  };
}

BasePcpHandler::StartOperationResult
P2pClusterPcpHandler::UpdateDiscoveryOptionsImpl(
    ClientProxy* client, absl::string_view service_id,
    absl::string_view local_endpoint_id, absl::string_view local_endpoint_info,
    const DiscoveryOptions& discovery_options) {
  DiscoveryOptions old_options = client->GetDiscoveryOptions();
  bool needs_restart = old_options.low_power != discovery_options.low_power;
  // ble
  if (NeedsToTurnOffDiscoveryMedium(Medium::BLE, old_options,
                                    discovery_options)) {
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
      ble_v2_medium_.StopScanning(std::string(service_id));
    } else {
      ble_medium_.StopScanning(std::string(service_id));
    }
    StartEndpointLostByMediumAlarms(client, Medium::BLE);
  }
  // bt classic
  if (NeedsToTurnOffDiscoveryMedium(Medium::BLUETOOTH, old_options,
                                    discovery_options) ||
      needs_restart) {
    bluetooth_medium_.StopDiscovery(std::string(service_id));
    StartEndpointLostByMediumAlarms(client, Medium::BLUETOOTH);
  }
  // wifi lan
  if (NeedsToTurnOffDiscoveryMedium(Medium::WIFI_LAN, old_options,
                                    discovery_options) ||
      needs_restart) {
    mediums_->GetWifiLan().StopDiscovery(std::string(service_id));
    StartEndpointLostByMediumAlarms(client, Medium::WIFI_LAN);
  }
  // restart
  std::vector<Medium> restarted_mediums;
  bool should_start_discovery = false;
  auto new_mediums = discovery_options.allowed;
  auto old_mediums = old_options.allowed;
  // ble
  if (new_mediums.ble) {
    should_start_discovery = true;
    if (old_mediums.ble) {
      restarted_mediums.push_back(Medium::BLE);
    } else {
      if (NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_connections_feature::
                  kEnableBleV2)) {
        if (StartBleV2Scanning(client, std::string(service_id),
                               discovery_options) !=
            location::nearby::proto::connections::UNKNOWN_MEDIUM) {
          restarted_mediums.push_back(Medium::BLE);
        } else {
          NEARBY_LOGS(WARNING) << "UpdateDiscoveryOptionsImpl: unable to "
                                  "restart blev2 scanning";
        }
      } else {
        if (StartBleScanning(
                client, std::string(service_id),
                discovery_options.fast_advertisement_service_uuid) !=
            location::nearby::proto::connections::UNKNOWN_MEDIUM) {
          restarted_mediums.push_back(Medium::BLE);
        } else {
          NEARBY_LOGS(WARNING)
              << "UpdateDiscoveryOptionsImpl: unable to restart ble scanning";
        }
      }
    }
  }
  // bt classic
  if (new_mediums.bluetooth && !discovery_options.low_power) {
    should_start_discovery = true;
    if (!needs_restart && old_mediums.bluetooth) {
      restarted_mediums.push_back(Medium::BLUETOOTH);
    } else {
      if (NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_connections_feature::
                  kDisableBluetoothClassicScanning)) {
        StartBluetoothDiscoveryWithPause(client, std::string(service_id),
                                         discovery_options, restarted_mediums);
      } else {
        if (StartBluetoothDiscovery(client, std::string(service_id)) !=
            location::nearby::proto::connections::UNKNOWN_MEDIUM) {
          restarted_mediums.push_back(Medium::BLUETOOTH);
        } else {
          NEARBY_LOGS(WARNING)
              << "UpdateDiscoveryOptionsImpl: unable to restart bt scanning";
        }
      }
    }
  }
  // wifi lan
  if (new_mediums.wifi_lan && !discovery_options.low_power) {
    should_start_discovery = true;
    if (!needs_restart && old_mediums.wifi_lan) {
      restarted_mediums.push_back(Medium::WIFI_LAN);
    } else {
      if (StartWifiLanDiscovery(client, std::string(service_id)) !=
          location::nearby::proto::connections::UNKNOWN_MEDIUM) {
        restarted_mediums.push_back(Medium::WIFI_LAN);
      } else {
        NEARBY_LOGS(WARNING) << "UpdateDiscoveryOptionsImpl: unable to restart "
                                "wifi lan scanning";
      }
    }
  }
  if (restarted_mediums.empty() && should_start_discovery) {
    // All radios failed to start.
    return StartOperationResult{.status = {Status::kError},
                                .mediums = restarted_mediums};
  }
  return StartOperationResult{.status = {Status::kSuccess},
                              .mediums = restarted_mediums};
}

void P2pClusterPcpHandler::BluetoothConnectionAcceptedHandler(
    ClientProxy* client, absl::string_view local_endpoint_info,
    NearbyDevice::Type device_type, const std::string& service_id,
    BluetoothSocket socket) {
  if (!socket.IsValid()) {
    NEARBY_LOGS(WARNING) << "Invalid socket in accept callback("
                         << absl::BytesToHexString(local_endpoint_info)
                         << "), client=" << client->GetClientId();
    return;
  }
  RunOnPcpHandlerThread(
      "p2p-bt-on-incoming-connection",
      [this, client, service_id, socket = std::move(socket), device_type]()
          RUN_ON_PCP_HANDLER_THREAD() mutable {
            std::string remote_device_name = socket.GetRemoteDevice().GetName();
            auto channel = std::make_unique<BluetoothEndpointChannel>(
                service_id,
                /*channel_name=*/remote_device_name, socket);
            ByteArray remote_device_info{remote_device_name};

            OnIncomingConnection(client, remote_device_info, std::move(channel),
                                 Medium::BLUETOOTH, device_type);
          });
}

Medium P2pClusterPcpHandler::StartBluetoothAdvertising(
    ClientProxy* client, const std::string& service_id,
    const ByteArray& service_id_hash, const std::string& local_endpoint_id,
    const ByteArray& local_endpoint_info, WebRtcState web_rtc_state) {
  // Start listening for connections before advertising in case a connection
  // request comes in very quickly.
  NEARBY_LOGS(INFO)
      << "P2pClusterPcpHandler::StartBluetoothAdvertising: service="
      << service_id << ": start";
  if (!bluetooth_medium_.IsAcceptingConnections(service_id)) {
    if (!bluetooth_radio_.Enable() ||
        !bluetooth_medium_.StartAcceptingConnections(
            service_id,
            absl::bind_front(
                &P2pClusterPcpHandler::BluetoothConnectionAcceptedHandler, this,
                client, local_endpoint_info.AsStringView(),
                NearbyDevice::Type::kConnectionsDevice))) {
      NEARBY_LOGS(WARNING)
          << "In StartBluetoothAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start listening for incoming Bluetooth "
             "connections to service_id="
          << service_id;
      return location::nearby::proto::connections::UNKNOWN_MEDIUM;
    }
    NEARBY_LOGS(INFO)
        << "In StartBluetoothAdvertising("
        << absl::BytesToHexString(local_endpoint_info.data())
        << "), client=" << client->GetClientId()
        << " started listening for incoming Bluetooth connections to "
           "service_id="
        << service_id;
  }

  // Generate a BluetoothDeviceName with which to become Bluetooth
  // discoverable.
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
    return location::nearby::proto::connections::UNKNOWN_MEDIUM;
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
    return location::nearby::proto::connections::UNKNOWN_MEDIUM;
  }
  NEARBY_LOGS(INFO)
      << "In StartBluetoothAdvertising("
      << absl::BytesToHexString(local_endpoint_info.data())
      << "), client=" << client->GetClientId()
      << " started Bluetooth advertising with BluetoothDeviceName "
      << device_name;
  return location::nearby::proto::connections::BLUETOOTH;
}

Medium P2pClusterPcpHandler::StartBluetoothDiscovery(
    ClientProxy* client, const std::string& service_id) {
  if (bluetooth_radio_.Enable() &&
      bluetooth_medium_.StartDiscovery(
          service_id,
          {
              .device_discovered_cb = absl::bind_front(
                  &P2pClusterPcpHandler::BluetoothDeviceDiscoveredHandler, this,
                  client, service_id),
              .device_name_changed_cb = absl::bind_front(
                  &P2pClusterPcpHandler::BluetoothNameChangedHandler, this,
                  client, service_id),
              .device_lost_cb = absl::bind_front(
                  &P2pClusterPcpHandler::BluetoothDeviceLostHandler, this,
                  client, service_id),
          })) {
    NEARBY_LOGS(INFO) << "In StartBluetoothDiscovery(), client="
                      << client->GetClientId()
                      << " started scanning for Bluetooth for service_id="
                      << service_id;
    return location::nearby::proto::connections::BLUETOOTH;
  } else {
    NEARBY_LOGS(INFO) << "In StartBluetoothDiscovery(), client="
                      << client->GetClientId()
                      << " couldn't start scanning on Bluetooth for service_id="
                      << service_id;
    return location::nearby::proto::connections::UNKNOWN_MEDIUM;
  }
}

void P2pClusterPcpHandler::StartBluetoothDiscoveryWithPause(
    ClientProxy* client, const std::string& service_id,
    const DiscoveryOptions& discovery_options,
    std::vector<Medium>& mediums_started_successfully) {
  if (bluetooth_radio_.IsEnabled()) {
    if (ble_v2_medium_.IsExtendedAdvertisementsAvailable() &&
        std::find(mediums_started_successfully.begin(),
                  mediums_started_successfully.end(),
                  location::nearby::proto::connections::BLE) !=
            mediums_started_successfully.end()) {
      if (bluetooth_medium_.IsDiscovering(service_id)) {
        // If we are already discovering, we don't need to start again.
        Medium bluetooth_medium = StartBluetoothDiscovery(client, service_id);
        if (bluetooth_medium !=
            location::nearby::proto::connections::UNKNOWN_MEDIUM) {
          NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::"
                               "StartBluetoothDiscoveryWithPause: BT added";
          mediums_started_successfully.push_back(bluetooth_medium);
          bluetooth_classic_client_id_to_service_id_map_.insert(
              {client->GetClientId(), service_id});
        }
      } else {
        NEARBY_LOGS(INFO) << "Pause bluetooth discovery for service id : "
                          << service_id;
        paused_bluetooth_clients_discoveries_.insert({service_id, client});
      }
    } else {
      // Always start bluetooth discovery if BLE doesn't support extended
      // advertisements.
      Medium bluetooth_medium = StartBluetoothDiscovery(client, service_id);
      if (bluetooth_medium !=
          location::nearby::proto::connections::UNKNOWN_MEDIUM) {
        NEARBY_LOGS(INFO) << "P2pClusterPcpHandler::"
                             "StartBluetoothDiscoveryWithPause: BT added";
        mediums_started_successfully.push_back(bluetooth_medium);
        bluetooth_classic_client_id_to_service_id_map_.insert(
            {client->GetClientId(), service_id});
      }
    }
  } else {
    NEARBY_LOGS(WARNING) << "Ignore to discover on bluetooth for service id: "
                         << service_id
                         << " because bluetooth is disabled or low power mode.";
  }
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::BluetoothConnectImpl(
    ClientProxy* client, BluetoothEndpoint* endpoint) {
  NEARBY_VLOG(1) << "Client " << client->GetClientId()
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

  auto channel = std::make_unique<BluetoothEndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id,
      bluetooth_socket);
  NEARBY_VLOG(1) << "Client" << client->GetClientId()
                 << " created Bluetooth endpoint channel to endpoint(id="
                 << endpoint->endpoint_id << ").";
  client->SetBluetoothMacAddress(endpoint->endpoint_id, device.GetMacAddress());
  return BasePcpHandler::ConnectImplResult{
      .medium = Medium::BLUETOOTH,
      .status = {Status::kSuccess},
      .endpoint_channel = std::move(channel),
  };
}

void P2pClusterPcpHandler::BleConnectionAcceptedHandler(
    ClientProxy* client, absl::string_view local_endpoint_info,
    NearbyDevice::Type device_type, BleSocket socket,
    const std::string& service_id) {
  if (!socket.IsValid()) {
    NEARBY_LOGS(WARNING) << "Invalid socket in accept callback("
                         << absl::BytesToHexString(local_endpoint_info)
                         << "), client=" << client->GetClientId();
    return;
  }
  RunOnPcpHandlerThread(
      "p2p-ble-on-incoming-connection",
      [this, client, service_id, socket = std::move(socket), device_type]()
          RUN_ON_PCP_HANDLER_THREAD() mutable {
            std::string remote_peripheral_name =
                socket.GetRemotePeripheral().GetName();
            auto channel = std::make_unique<BleEndpointChannel>(
                service_id,
                /*channel_name=*/remote_peripheral_name, socket);
            ByteArray remote_peripheral_info =
                socket.GetRemotePeripheral().GetAdvertisementBytes(service_id);

            OnIncomingConnection(client, remote_peripheral_info,
                                 std::move(channel), Medium::BLE, device_type);
          });
}

Medium P2pClusterPcpHandler::StartBleAdvertising(
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
            service_id, absl::bind_front(
                            &P2pClusterPcpHandler::BleConnectionAcceptedHandler,
                            this, client, local_endpoint_info.AsStringView(),
                            NearbyDevice::Type::kConnectionsDevice))) {
      NEARBY_LOGS(WARNING)
          << "In StartBleAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start accepting for incoming BLE connections to "
             "service_id="
          << service_id;
      return location::nearby::proto::connections::UNKNOWN_MEDIUM;
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
              service_id,
              absl::bind_front(
                  &P2pClusterPcpHandler::BluetoothConnectionAcceptedHandler,
                  this, client, local_endpoint_info.AsStringView(),
                  NearbyDevice::Type::kConnectionsDevice))) {
        NEARBY_LOGS(WARNING)
            << "In BT StartBleAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " failed to start accepting for incoming BLE connections to "
               "service_id="
            << service_id;
        ble_medium_.StopAcceptingConnections(service_id);
        return location::nearby::proto::connections::UNKNOWN_MEDIUM;
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
                         << " failed to create an advertisement.";
    ble_medium_.StopAcceptingConnections(service_id);
    return location::nearby::proto::connections::UNKNOWN_MEDIUM;
  }

  NEARBY_LOGS(INFO) << "In StartBleAdvertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId()
                    << " generated BleAdvertisement with service_id="
                    << service_id << ", bytes: "
                    << absl::BytesToHexString(advertisement_bytes.data());

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
    return location::nearby::proto::connections::UNKNOWN_MEDIUM;
  }
  NEARBY_LOGS(INFO) << "In startBleAdvertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << ", fast_advertisement: " << fast_advertisement
                    << "), client=" << client->GetClientId()
                    << " started BLE Advertising with BleAdvertisement "
                    << absl::BytesToHexString(advertisement_bytes.data());
  return location::nearby::proto::connections::BLE;
}

Medium P2pClusterPcpHandler::StartBleScanning(
    ClientProxy* client, const std::string& service_id,
    const std::string& fast_advertisement_service_uuid) {
  if (bluetooth_radio_.Enable() &&
      ble_medium_.StartScanning(
          service_id, fast_advertisement_service_uuid,
          {
              .peripheral_discovered_cb = absl::bind_front(
                  &P2pClusterPcpHandler::BlePeripheralDiscoveredHandler, this,
                  client),
              .peripheral_lost_cb = absl::bind_front(
                  &P2pClusterPcpHandler::BlePeripheralLostHandler, this,
                  client),
          })) {
    NEARBY_LOGS(INFO)
        << "In StartBleScanning(), client=" << client->GetClientId()
        << " started scanning for BLE advertisements for service_id="
        << service_id;
    return location::nearby::proto::connections::BLE;
  } else {
    NEARBY_LOGS(INFO) << "In StartBleScanning(), client="
                      << client->GetClientId()
                      << " couldn't start scanning on BLE for service_id="
                      << service_id;
    return location::nearby::proto::connections::UNKNOWN_MEDIUM;
  }
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::BleConnectImpl(
    ClientProxy* client, BleEndpoint* endpoint) {
  NEARBY_VLOG(1) << "Client " << client->GetClientId()
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

  auto channel = std::make_unique<BleEndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id, ble_socket);

  return BasePcpHandler::ConnectImplResult{
      .medium = Medium::BLE,
      .status = {Status::kSuccess},
      .endpoint_channel = std::move(channel),
  };
}

void P2pClusterPcpHandler::BleV2ConnectionAcceptedHandler(
    ClientProxy* client, absl::string_view local_endpoint_info,
    NearbyDevice::Type device_type, BleV2Socket socket,
    const std::string& service_id) {
  if (!socket.IsValid()) {
    NEARBY_LOGS(WARNING) << "Invalid socket in accept callback("
                         << absl::BytesToHexString(local_endpoint_info)
                         << "), client=" << client->GetClientId();
    return;
  }
  RunOnPcpHandlerThread(
      "p2p-ble-on-incoming-connection",
      [this, client, service_id, device_type,
       socket = std::move(socket)]() RUN_ON_PCP_HANDLER_THREAD() mutable {
        ByteArray remote_peripheral_info = socket.GetRemotePeripheral().GetId();
        auto channel = std::make_unique<BleV2EndpointChannel>(
            service_id, std::string(remote_peripheral_info), socket);

        OnIncomingConnection(client, remote_peripheral_info, std::move(channel),
                             Medium::BLE, device_type);
      });
}

Medium P2pClusterPcpHandler::StartBleV2Advertising(
    ClientProxy* client, const std::string& service_id,
    const std::string& local_endpoint_id, const ByteArray& local_endpoint_info,
    const AdvertisingOptions& advertising_options, WebRtcState web_rtc_state) {
  // Start listening for connections before advertising in case a connection
  // request comes in very quickly. BLE allows connecting over BLE itself, as
  // well as advertising the Bluetooth MAC address to allow connecting over
  // Bluetooth Classic.
  NEARBY_LOGS(INFO)
      << "P2pClusterPcpHandler::StartBleV2Advertising: service_id="
      << service_id << " : start";
  if (!ble_v2_medium_.IsAcceptingConnections(service_id)) {
    if (!bluetooth_radio_.Enable() ||
        !ble_v2_medium_.StartAcceptingConnections(
            service_id,
            absl::bind_front(
                &P2pClusterPcpHandler::BleV2ConnectionAcceptedHandler, this,
                client, local_endpoint_info.AsStringView(),
                NearbyDevice::Type::kConnectionsDevice))) {
      NEARBY_LOGS(WARNING)
          << "In StartBleV2Advertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start accepting for incoming BLE connections to "
             "service_id="
          << service_id;
      return location::nearby::proto::connections::UNKNOWN_MEDIUM;
    }
    NEARBY_LOGS(INFO)
        << "In StartBleV2Advertising("
        << absl::BytesToHexString(local_endpoint_info.data())
        << "), client=" << client->GetClientId()
        << " started accepting for incoming BLE connections to service_id="
        << service_id;
  }

  PowerLevel power_level = advertising_options.low_power
                               ? PowerLevel::kLowPower
                               : PowerLevel::kHighPower;
  if (ShouldAdvertiseBluetoothMacOverBle(power_level) ||
      ShouldAcceptBluetoothConnections(advertising_options)) {
    if (bluetooth_medium_.IsAvailable() &&
        !bluetooth_medium_.IsAcceptingConnections(service_id)) {
      if (!bluetooth_radio_.Enable() ||
          !bluetooth_medium_.StartAcceptingConnections(
              service_id,
              absl::bind_front(
                  &P2pClusterPcpHandler::BluetoothConnectionAcceptedHandler,
                  this, client, local_endpoint_info.AsStringView(),
                  NearbyDevice::Type::kConnectionsDevice))) {
        NEARBY_LOGS(WARNING)
            << "In BT StartBleV2Advertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " failed to start accepting for incoming BLE connections to "
               "service_id="
            << service_id;
        ble_v2_medium_.StopAcceptingConnections(service_id);
        return location::nearby::proto::connections::UNKNOWN_MEDIUM;
      }
      NEARBY_LOGS(INFO)
          << "In BT StartBleV2Advertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " started accepting for incoming BLE connections to service_id="
          << service_id;
    }
  }

  NEARBY_LOGS(INFO) << "In StartBleV2Advertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId()
                    << " start to generate BleAdvertisement with service_id="
                    << service_id
                    << ", local endpoint_id=" << local_endpoint_id;
  // Generate a BleAdvertisement. If a fast advertisement service UUID was
  // provided, create a fast BleAdvertisement.
  ByteArray advertisement_bytes;
  bool fast_advertisement =
      !advertising_options.fast_advertisement_service_uuid.empty();
  if (fast_advertisement) {
    advertisement_bytes = ByteArray(
        BleAdvertisement(kBleAdvertisementVersion, GetPcp(), local_endpoint_id,
                         local_endpoint_info, /*uwb_address=*/ByteArray{}));
  } else {
    const ByteArray service_id_hash =
        GenerateHash(service_id, BleAdvertisement::kServiceIdHashLength);
    std::string bluetooth_mac_address;
    if (bluetooth_medium_.IsAvailable() &&
        ShouldAdvertiseBluetoothMacOverBle(power_level))
      bluetooth_mac_address = bluetooth_medium_.GetMacAddress();

    advertisement_bytes = ByteArray(BleAdvertisement(
        kBleAdvertisementVersion, GetPcp(), service_id_hash, local_endpoint_id,
        local_endpoint_info, bluetooth_mac_address,
        /*uwb_address=*/ByteArray{}, web_rtc_state));
  }
  if (advertisement_bytes.Empty()) {
    NEARBY_LOGS(WARNING) << "In StartBleV2Advertising("
                         << absl::BytesToHexString(local_endpoint_info.data())
                         << "), client=" << client->GetClientId()
                         << " failed to create an advertisement.";
    ble_v2_medium_.StopAcceptingConnections(service_id);
    return location::nearby::proto::connections::UNKNOWN_MEDIUM;
  }

  NEARBY_LOGS(INFO) << "In StartBleV2Advertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId()
                    << " generated BleAdvertisement with service_id="
                    << service_id;

  if (!ble_v2_medium_.StartAdvertising(
          service_id, advertisement_bytes, power_level,
          !advertising_options.fast_advertisement_service_uuid.empty())) {
    NEARBY_LOGS(WARNING)
        << "In StartBleV2Advertising("
        << absl::BytesToHexString(local_endpoint_info.data())
        << "), client=" << client->GetClientId()
        << " couldn't start BLE Advertising with BleAdvertisement "
        << absl::BytesToHexString(advertisement_bytes.data());
    ble_v2_medium_.StopAcceptingConnections(service_id);
    return location::nearby::proto::connections::UNKNOWN_MEDIUM;
  }
  NEARBY_LOGS(INFO) << "In StartBleV2Advertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId()
                    << " started BLE Advertising with BleAdvertisement "
                    << absl::BytesToHexString(advertisement_bytes.data());
  return location::nearby::proto::connections::BLE;
}

Medium P2pClusterPcpHandler::StartBleV2Scanning(
    ClientProxy* client, const std::string& service_id,
    const DiscoveryOptions& discovery_options) {
  PowerLevel power_level = discovery_options.low_power ? PowerLevel::kLowPower
                                                       : PowerLevel::kHighPower;
  if (bluetooth_radio_.Enable() &&
      ble_v2_medium_.StartScanning(
          service_id, power_level,
          {
              .peripheral_discovered_cb = absl::bind_front(
                  &P2pClusterPcpHandler::BleV2PeripheralDiscoveredHandler, this,
                  client),
              .peripheral_lost_cb = absl::bind_front(
                  &P2pClusterPcpHandler::BleV2PeripheralLostHandler, this,
                  client),
              .instant_lost_cb = absl::bind_front(
                  &P2pClusterPcpHandler::BleV2InstantLostHandler, this, client),
              .legacy_device_discovered_cb = absl::bind_front(
                  &P2pClusterPcpHandler::BleV2LegacyDeviceDiscoveredHandler,
                  this),
          })) {
    NEARBY_LOGS(INFO)
        << "In StartBleV2Scanning(), client=" << client->GetClientId()
        << " started scanning for BLE advertisements for service_id="
        << service_id;
    return location::nearby::proto::connections::BLE;
  }
  NEARBY_LOGS(INFO) << "In StartBleV2Scanning(), client="
                    << client->GetClientId()
                    << " couldn't start scanning on BLE for service_id="
                    << service_id;

  return location::nearby::proto::connections::UNKNOWN_MEDIUM;
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::BleV2ConnectImpl(
    ClientProxy* client, BleV2Endpoint* endpoint) {
  NEARBY_VLOG(1) << "Client " << client->GetClientId()
                 << " is attempting to connect to endpoint(id="
                 << endpoint->endpoint_id << ") over BLE.";

  BleV2Peripheral& peripheral = endpoint->ble_peripheral;

  BleV2Socket ble_socket = ble_v2_medium_.Connect(
      endpoint->service_id, peripheral,
      client->GetCancellationFlag(endpoint->endpoint_id));
  if (!ble_socket.IsValid()) {
    NEARBY_LOGS(ERROR)
        << "In BleV2ConnectImpl(), failed to connect to BLE device "
        << absl::BytesToHexString(peripheral.GetId().data())
        << " for endpoint(id=" << endpoint->endpoint_id << ").";
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kBleError},
    };
  }

  auto channel = std::make_unique<BleV2EndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id, ble_socket);

  return BasePcpHandler::ConnectImplResult{
      .medium = Medium::BLE,
      .status = {Status::kSuccess},
      .endpoint_channel = std::move(channel),
  };
}

void P2pClusterPcpHandler::WifiLanConnectionAcceptedHandler(
    ClientProxy* client, absl::string_view local_endpoint_id,
    absl::string_view local_endpoint_info, NearbyDevice::Type device_type,
    const std::string& service_id, WifiLanSocket socket) {
  if (!socket.IsValid()) {
    NEARBY_LOGS(WARNING) << "Invalid socket in accept callback("
                         << absl::BytesToHexString(local_endpoint_info)
                         << "), client=" << client->GetClientId();
    return;
  }
  RunOnPcpHandlerThread(
      "p2p-wifi-on-incoming-connection",
      [this, client, local_endpoint_id, service_id, device_type,
       socket = std::move(socket)]() RUN_ON_PCP_HANDLER_THREAD() mutable {
        std::string remote_service_name = std::string(local_endpoint_id);
        auto channel = std::make_unique<WifiLanEndpointChannel>(
            service_id, /*channel_name=*/remote_service_name, socket);
        ByteArray remote_service_name_byte{remote_service_name};

        OnIncomingConnection(client, remote_service_name_byte,
                             std::move(channel), Medium::WIFI_LAN, device_type);
      });
}

Medium P2pClusterPcpHandler::StartWifiLanAdvertising(
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
            absl::bind_front(
                &P2pClusterPcpHandler::WifiLanConnectionAcceptedHandler, this,
                client, local_endpoint_id, local_endpoint_info.AsStringView(),
                NearbyDevice::Type::kConnectionsDevice))) {
      NEARBY_LOGS(WARNING)
          << "In StartWifiLanAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start listening for incoming WifiLan connections "
             "to service_id="
          << service_id;
      return location::nearby::proto::connections::UNKNOWN_MEDIUM;
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
    return location::nearby::proto::connections::UNKNOWN_MEDIUM;
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
    return location::nearby::proto::connections::UNKNOWN_MEDIUM;
  }
  NEARBY_LOGS(INFO) << "In StartWifiLanAdvertising("
                    << absl::BytesToHexString(local_endpoint_info.data())
                    << "), client=" << client->GetClientId()
                    << " advertised with WifiLanServiceInfo "
                    << nsd_service_info.GetServiceName();
  return location::nearby::proto::connections::WIFI_LAN;
}

Medium P2pClusterPcpHandler::StartWifiLanDiscovery(
    ClientProxy* client, const std::string& service_id) {
  if (wifi_lan_medium_.StartDiscovery(
          service_id,
          {
              .service_discovered_cb = absl::bind_front(
                  &P2pClusterPcpHandler::WifiLanServiceDiscoveredHandler, this,
                  client),
              .service_lost_cb = absl::bind_front(
                  &P2pClusterPcpHandler::WifiLanServiceLostHandler, this,
                  client),
          })) {
    NEARBY_LOGS(INFO) << "In StartWifiLanDiscovery(), client="
                      << client->GetClientId()
                      << " started scanning for Wifi devices for service_id="
                      << service_id;
    return location::nearby::proto::connections::WIFI_LAN;
  } else {
    NEARBY_LOGS(INFO) << "In StartWifiLanDiscovery(), client="
                      << client->GetClientId()
                      << " couldn't start scanning on Wifi for service_id="
                      << service_id;
    return location::nearby::proto::connections::UNKNOWN_MEDIUM;
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
  if (!socket.IsValid()) {
    NEARBY_LOGS(ERROR)
        << "In WifiLanConnectImpl(), failed to connect to service "
        << endpoint->service_info.GetServiceName()
        << " for endpoint(id=" << endpoint->endpoint_id << ").";
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kWifiLanError},
    };
  }
  NEARBY_LOGS(INFO) << "In WifiLanConnectImpl(), connect to service "
                    << " socket=" << &socket.GetImpl()
                    << " for endpoint(id=" << endpoint->endpoint_id << ").";

  auto channel = std::make_unique<WifiLanEndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id, socket);
  NEARBY_LOGS(INFO) << "Client " << client->GetClientId()
                    << " created WifiLan endpoint channel to endpoint(id="
                    << endpoint->endpoint_id << ").";
  return BasePcpHandler::ConnectImplResult{
      .medium = Medium::WIFI_LAN,
      .status = {Status::kSuccess},
      .endpoint_channel = std::move(channel),
  };
}

}  // namespace connections
}  // namespace nearby
