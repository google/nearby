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
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "connections/advertising_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/awdl_endpoint_channel.h"
#include "connections/implementation/base_pcp_handler.h"
#include "connections/implementation/ble_advertisement.h"
#include "connections/implementation/ble_endpoint_channel.h"
#include "connections/implementation/ble_l2cap_endpoint_channel.h"
#include "connections/implementation/ble_v2_endpoint_channel.h"
#include "connections/implementation/bluetooth_device_name.h"
#include "connections/implementation/bluetooth_endpoint_channel.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/injected_bluetooth_device_store.h"
#include "connections/implementation/mediums/advertisements/advertisement_util.h"
#include "connections/implementation/mediums/advertisements/dct_advertisement.h"
#include "connections/implementation/mediums/ble_v2.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "connections/implementation/mediums/bluetooth_classic.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/pcp.h"
#include "connections/implementation/pcp_handler.h"
#include "connections/implementation/webrtc_state.h"
#include "connections/implementation/wifi_lan_endpoint_channel.h"
#include "connections/implementation/wifi_lan_service_info.h"
#include "connections/medium_selector.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/power_level.h"
#include "connections/status.h"
#include "connections/v3/connection_listening_options.h"
#include "internal/flags/nearby_flags.h"
#include "internal/interop/device.h"
#include "internal/platform/awdl.h"
#include "internal/platform/ble.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/bluetooth_classic.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/expected.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/logging.h"
#include "internal/platform/mac_address.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/os_name.h"
#include "internal/platform/types.h"
#include "internal/platform/wifi_lan.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {

namespace {
using ::location::nearby::analytics::proto::ConnectionsLog;
using ::location::nearby::proto::connections::OperationResultCode;
using ::location::nearby::proto::connections::Medium::AWDL;
using ::location::nearby::proto::connections::Medium::BLE;
using ::location::nearby::proto::connections::Medium::BLUETOOTH;
using ::location::nearby::proto::connections::Medium::UNKNOWN_MEDIUM;
using ::location::nearby::proto::connections::Medium::WEB_RTC;
using ::location::nearby::proto::connections::Medium::WIFI_LAN;

}  // namespace

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
      awdl_medium_(mediums->GetAwdl()),
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
  VLOG(1) << __func__;
  Shutdown();
}

// Returns a vector or mediums sorted in order or decreasing priority for
// all the supported mediums.
// Example: WiFi_LAN, WEB_RTC, BT, BLE
std::vector<Medium> P2pClusterPcpHandler::GetConnectionMediumsByPriority() {
  std::vector<Medium> mediums;
  if (wifi_lan_medium_.IsAvailable()) {
    mediums.push_back(WIFI_LAN);
  }
  if (webrtc_medium_.IsAvailable()) {
    mediums.push_back(WEB_RTC);
  }
  if (bluetooth_medium_.IsAvailable()) {
    mediums.push_back(BLUETOOTH);
  }
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
    if (ble_v2_medium_.IsAvailable()) {
      mediums.push_back(BLE);
    }
  } else {
    if (ble_medium_.IsAvailable()) {
      mediums.push_back(BLE);
    }
  }

  return mediums;
}

Medium P2pClusterPcpHandler::GetDefaultUpgradeMedium() { return WIFI_LAN; }

BasePcpHandler::StartOperationResult P2pClusterPcpHandler::StartAdvertisingImpl(
    ClientProxy* client, const std::string& service_id,
    const std::string& local_endpoint_id, const ByteArray& local_endpoint_info,
    const AdvertisingOptions& advertising_options) {
  std::vector<Medium> mediums_started_successfully;
  std::vector<ConnectionsLog::OperationResultWithMedium>
      operation_result_with_mediums;

  WebRtcState web_rtc_state{WebRtcState::kUnconnectable};

  // AWDL
  if (advertising_options.allowed.awdl &&
      NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableAwdl)) {
    ErrorOr<Medium> awdl_result =
        StartAwdlAdvertising(client, service_id, local_endpoint_id,
                             local_endpoint_info, web_rtc_state);

    Medium awdl_medium = UNKNOWN_MEDIUM;
    if (awdl_result.has_value()) {
      awdl_medium = awdl_result.value();
    }
    if (awdl_medium != UNKNOWN_MEDIUM) {
      VLOG(1) << "P2pClusterPcpHandler::StartAdvertisingImpl: Awdl added";
      mediums_started_successfully.push_back(awdl_medium);
    }
    std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
        operation_result_with_medium = GetOperationResultWithMediumByResultCode(
            client, AWDL, /*update_index=*/0,
            awdl_result.has_error()
                ? awdl_result.error().operation_result_code().value()
                : OperationResultCode::DETAIL_SUCCESS);
    operation_result_with_mediums.push_back(*operation_result_with_medium);
  }

  // WifiLan
  if (advertising_options.allowed.wifi_lan) {
    ErrorOr<Medium> wifi_lan_result =
        StartWifiLanAdvertising(client, service_id, local_endpoint_id,
                                local_endpoint_info, web_rtc_state);
    Medium wifi_lan_medium = UNKNOWN_MEDIUM;
    if (wifi_lan_result.has_value()) {
      wifi_lan_medium = wifi_lan_result.value();
    }
    if (wifi_lan_medium != UNKNOWN_MEDIUM) {
      VLOG(1) << "P2pClusterPcpHandler::StartAdvertisingImpl: WifiLan added";
      mediums_started_successfully.push_back(wifi_lan_medium);
    }
    std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
        operation_result_with_medium = GetOperationResultWithMediumByResultCode(
            client, WIFI_LAN, /*update_index=*/0,
            wifi_lan_result.has_error()
                ? wifi_lan_result.error().operation_result_code().value()
                : OperationResultCode::DETAIL_SUCCESS);
    operation_result_with_mediums.push_back(*operation_result_with_medium);
  }

  if (advertising_options.allowed.bluetooth) {
    const ByteArray bluetooth_hash =
        GenerateHash(service_id, BluetoothDeviceName::kServiceIdHashLength);
    ErrorOr<Medium> bluetooth_result = StartBluetoothAdvertising(
        client, service_id, bluetooth_hash, local_endpoint_id,
        local_endpoint_info, web_rtc_state);
    Medium bluetooth_medium = UNKNOWN_MEDIUM;
    if (bluetooth_result.has_value()) {
      bluetooth_medium = bluetooth_result.value();
    }
    if (bluetooth_medium != UNKNOWN_MEDIUM) {
      LOG(INFO) << "P2pClusterPcpHandler::StartAdvertisingImpl: BT started";

      // TODO(hais): update this after ble_v2 refactor.
      if (api::ImplementationPlatform::GetCurrentOS() ==
              api::OSName::kChromeOS &&
          !NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_connections_feature::
                  kEnableBleV2)) {
        if (ble_medium_.StartLegacyAdvertising(
                service_id, local_endpoint_id,
                advertising_options.fast_advertisement_service_uuid)) {
          VLOG(1) << "P2pClusterPcpHandler::StartAdvertisingImpl: "
                     "Ble legacy started advertising";
          VLOG(1) << "P2pClusterPcpHandler::StartAdvertisingImpl: BT added";
          mediums_started_successfully.push_back(bluetooth_medium);
          bluetooth_classic_advertiser_client_id_ = client->GetClientId();
        } else {
          // TODO(hais): update this after ble_v2 refactor.
          LOG(WARNING) << "P2pClusterPcpHandler::StartAdvertisingImpl: "
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
          LOG(INFO) << __func__
                    << "Ble v2 started advertising for legacy device.";
          mediums_started_successfully.push_back(bluetooth_medium);
          VLOG(1) << __func__ << "After Ble v2, BT added";
          bluetooth_classic_advertiser_client_id_ = client->GetClientId();
        } else {
          LOG(WARNING) << "P2pClusterPcpHandler::StartAdvertisingImpl: "
                          "BLE legacy failed, revert BTC";
          bluetooth_medium_.TurnOffDiscoverability();
          bluetooth_medium_.StopAcceptingConnections(service_id);
        }
      } else {
        VLOG(1) << "P2pClusterPcpHandler::StartAdvertisingImpl: BT added";
        mediums_started_successfully.push_back(bluetooth_medium);
        bluetooth_classic_advertiser_client_id_ = client->GetClientId();
      }
    }
    std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
        operation_result_with_medium = GetOperationResultWithMediumByResultCode(
            client, BLUETOOTH,
            /*update_index=*/0,
            bluetooth_result.has_error()
                ? bluetooth_result.error().operation_result_code().value()
                : OperationResultCode::DETAIL_SUCCESS);
    operation_result_with_mediums.push_back(*operation_result_with_medium);
  }

  if (advertising_options.allowed.ble) {
    ErrorOr<Medium> ble_result = {Error(OperationResultCode::DETAIL_UNKNOWN)};
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
      ble_result = StartBleV2Advertising(client, service_id, local_endpoint_id,
                                         local_endpoint_info,
                                         advertising_options, web_rtc_state);
      if (ble_result.has_value() && ble_result.value() != UNKNOWN_MEDIUM) {
        VLOG(1) << "P2pClusterPcpHandler::StartAdvertisingImpl: Ble added";
        mediums_started_successfully.push_back(ble_result.value());
      }
    } else {
      ble_result = StartBleAdvertising(client, service_id, local_endpoint_id,
                                       local_endpoint_info, advertising_options,
                                       web_rtc_state);
      if (ble_result.has_value() && ble_result.value() != UNKNOWN_MEDIUM) {
        VLOG(1) << "P2pClusterPcpHandler::StartAdvertisingImpl: Ble added";
        mediums_started_successfully.push_back(ble_result.value());
      }
    }
    std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
        operation_result_with_medium = GetOperationResultWithMediumByResultCode(
            client, BLE, /*update_index=*/0,
            ble_result.has_error()
                ? ble_result.error().operation_result_code().value()
                : OperationResultCode::DETAIL_SUCCESS);
  }

  if (mediums_started_successfully.empty()) {
    LOG(ERROR) << "Failed StartAdvertising("
               << absl::BytesToHexString(local_endpoint_info.data())
               << ") for client=" << client->GetClientId();
    // TODO(edwinwu): Modify the status instead of kBluetoothError
    return {
        .status = {Status::kBluetoothError},
        .operation_result_with_mediums =
            std::move(operation_result_with_mediums),
    };
  }

  // The rest of the operations for startAdvertising() will continue
  // asynchronously via
  // IncomingBluetoothConnectionProcessor.onIncomingBluetoothConnection(), so
  // leave it to that to signal any errors that may occur.
  return {
      .status = {Status::kSuccess},
      .mediums = std::move(mediums_started_successfully),
      .operation_result_with_mediums = std::move(operation_result_with_mediums),
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
    LOG(INFO) << "Skipped BT TurnOffDiscoverability for client="
              << client->GetClientId()
              << ", client that turned on discoverability is "
              << bluetooth_classic_advertiser_client_id_;
  }

  bluetooth_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());

  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
    ble_v2_medium_.StopAdvertising(client->GetAdvertisingServiceId());
    ble_v2_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kEnableBleL2cap)) {
      ble_v2_medium_.StopAcceptingL2capConnections(
          client->GetAdvertisingServiceId());
    }
  } else {
    ble_medium_.StopAdvertising(client->GetAdvertisingServiceId());
    ble_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());
  }

  wifi_lan_medium_.StopAdvertising(client->GetAdvertisingServiceId());
  wifi_lan_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());

  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableAwdl)) {
    awdl_medium_.StopAdvertising(client->GetAdvertisingServiceId());
    awdl_medium_.StopAcceptingConnections(client->GetAdvertisingServiceId());
  }

  return {Status::kSuccess};
}

bool P2pClusterPcpHandler::IsRecognizedBluetoothEndpoint(
    const std::string& name_string, const std::string& service_id,
    const BluetoothDeviceName& name) const {
  if (!name.IsValid()) {
    LOG(INFO) << name_string << " doesn't have any endpoint id, discarding.";
    return false;
  }

  if (name.GetPcp() != GetPcp()) {
    LOG(INFO) << name_string << " doesn't match on Pcp; expected "
              << PcpToStrategy(GetPcp()).GetName() << ", found "
              << PcpToStrategy(name.GetPcp()).GetName();
    return false;
  }

  ByteArray expected_service_id_hash =
      GenerateHash(service_id, BluetoothDeviceName::kServiceIdHashLength);

  if (name.GetServiceIdHash() != expected_service_id_hash) {
    LOG(INFO) << name_string
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
      [this, client, service_id, device]() RUN_ON_PCP_HANDLER_THREAD() {
        if (!device.IsValid()) {
          LOG(WARNING) << "BluetoothDeviceDiscoveredHandler: "
                          "Skipping the invalid Bluetooth device";
          return;
        }

        // Make sure we are still discovering before proceeding.
        if (!bluetooth_medium_.IsDiscovering(service_id) &&
            !client->GetDiscoveryOptions().is_out_of_band_connection) {
          LOG(WARNING) << "Skipping discovered Bluetooth device due to "
                          "no longer discovering.";
          return;
        }

        // Parse the Bluetooth device name.
        const std::string device_name_string = device.GetName();
        BluetoothDeviceName device_name(device_name_string);

        // Make sure the Bluetooth device name points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedBluetoothEndpoint(device_name_string, service_id,
                                           device_name)) {
          LOG(INFO) << "Found unrecognized BluetoothDeviceName "
                    << device_name_string;
          return;
        }

        // Report the discovered endpoint to the client.
        VLOG(1) << "Found BluetoothDeviceName " << device_name_string
                << " (with endpoint_id=" << device_name.GetEndpointId()
                << " and endpoint_info="
                << absl::BytesToHexString(device_name.GetEndpointInfo().data())
                << ").";
        OnEndpointFound(
            client,
            std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
                {device_name.GetEndpointId(), device_name.GetEndpointInfo(),
                 service_id, BLUETOOTH, device_name.GetWebRtcState()},
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
        if (!device.IsValid()) {
          LOG(WARNING) << "BluetoothNameChangedHandler: Skipping the "
                          "invalid Bluetooth device";
          return;
        }

        if (!bluetooth_medium_.IsDiscovering(service_id) &&
            !client->GetDiscoveryOptions().is_out_of_band_connection) {
          LOG(WARNING) << "Ignoring name changed Bluetooth device due to no "
                          "longer discovering.";
          return;
        }

        // Parse the Bluetooth device name.
        const std::string device_name_string = device.GetName();
        BluetoothDeviceName device_name(device_name_string);
        LOG(INFO) << "BT discovery handler (CHANGED) [client_id="
                  << client->GetClientId() << ", service_id=" << service_id
                  << "]: processing new name " << device_name_string;

        // Make sure the Bluetooth device name points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedBluetoothEndpoint(device_name_string, service_id,
                                           device_name)) {
          LOG(INFO) << "Found unrecognized BluetoothDeviceName "
                    << device_name_string;
          return;
        }

        // By this point, the BluetoothDevice passed to us has a different
        // name than what we may have discovered before. We need to iterate
        // over the found BluetoothEndpoints and compare their addresses to
        // see the devices are the same. We are not guaranteed to discover a
        // match, since the old name may not have been formatted for Nearby
        // Connections.
        for (auto endpoint : GetDiscoveredEndpoints(Medium::BLUETOOTH)) {
          BluetoothEndpoint* bluetoothEndpoint =
              static_cast<BluetoothEndpoint*>(endpoint);
          LOG(INFO)
              << "BT discovery handler (CHANGED) [client_id="
              << client->GetClientId() << ", service_id=" << service_id
              << "]: comparing MAC addresses with existing endpoint "
              << bluetoothEndpoint->bluetooth_device.GetName()
              << ". They have MAC address "
              << bluetoothEndpoint->bluetooth_device.GetAddress().ToString()
              << " and the new endpoint has MAC address "
              << device.GetAddress().ToString();
          if (bluetoothEndpoint->bluetooth_device.GetAddress() ==
              device.GetAddress()) {
            // Report the BluetoothEndpoint as lost to the client.
            VLOG(1) << "Reporting lost BluetoothDevice "
                    << bluetoothEndpoint->bluetooth_device.GetName()
                    << ", due to device name change.";
            OnEndpointLost(client, *endpoint);
            break;
          }
        }

        // Report the discovered endpoint to the client.
        VLOG(1) << "Found BluetoothDeviceName " << device_name_string
                << " (with endpoint_id=" << device_name.GetEndpointId()
                << " and endpoint_info="
                << absl::BytesToHexString(device_name.GetEndpointInfo().data())
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
  if (!device.IsValid()) {
    LOG(WARNING)
        << "BluetoothDeviceLostHandler:Skipping the invalid Bluetooth device";
    return;
  }

  const std::string& device_name_string = device.GetName();
  RunOnPcpHandlerThread(
      "p2p-bt-device-lost", [this, client, service_id,
                             device_name_string]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!bluetooth_medium_.IsDiscovering(service_id) &&
            !client->GetDiscoveryOptions().is_out_of_band_connection) {
          LOG(WARNING) << "Ignoring lost Bluetooth device due to no "
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
        VLOG(1) << "Processing lost BluetoothDeviceName " << device_name_string;
        OnEndpointLost(client, DiscoveredEndpoint{device_name.GetEndpointId(),
                                                  device_name.GetEndpointInfo(),
                                                  service_id, BLUETOOTH,
                                                  WebRtcState::kUndefined});
      });
}

bool P2pClusterPcpHandler::IsRecognizedBleEndpoint(
    const std::string& service_id,
    const BleAdvertisement& advertisement) const {
  if (advertisement.GetPcp() != GetPcp()) {
    LOG(INFO) << "BleAdvertisement doesn't match on Pcp; expected "
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
      LOG(INFO)
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
          LOG(WARNING) << "Skipping discovery of BleAdvertisement header "
                       << absl::BytesToHexString(advertisement_bytes.data())
                       << " because we are no longer discovering.";
          return;
        }

        auto ble_status_or = BleAdvertisement::CreateBleAdvertisement(
            fast_advertisement, advertisement_bytes);
        if (!ble_status_or.ok()) {
          LOG(ERROR) << ble_status_or.status();
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

        StopEndpointLostByMediumAlarm(advertisement.GetEndpointId(), BLE);

        // Report the discovered endpoint to the client.
        VLOG(1) << "Found BleAdvertisement "
                << absl::BytesToHexString(advertisement_bytes.data())
                << " (with endpoint_id=" << advertisement.GetEndpointId()
                << ", and endpoint_info="
                << absl::BytesToHexString(
                       advertisement.GetEndpointInfo().data())
                << ").";
        OnEndpointFound(
            client,
            std::make_shared<BleEndpoint>(BleEndpoint{
                {advertisement.GetEndpointId(), advertisement.GetEndpointInfo(),
                 service_id, BLE, advertisement.GetWebRtcState()},
                peripheral,
            }));

        // Make sure we can connect to this device via Classic Bluetooth.
        MacAddress remote_bluetooth_mac_address =
            advertisement.GetBluetoothMacAddress();
        if (!remote_bluetooth_mac_address.IsSet()) {
          LOG(INFO)
              << "No Bluetooth Classic MAC address found in advertisement.";
          return;
        }

        BluetoothDevice remote_bluetooth_device =
            bluetooth_medium_.GetRemoteDevice(remote_bluetooth_mac_address);
        if (!remote_bluetooth_device.IsValid()) {
          LOG(INFO)
              << "A valid Bluetooth device could not be derived from the MAC "
                 "address "
              << remote_bluetooth_mac_address.ToString();
          return;
        }

        StopEndpointLostByMediumAlarm(advertisement.GetEndpointId(), BLUETOOTH);
        OnEndpointFound(client,
                        std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
                            {
                                advertisement.GetEndpointId(),
                                advertisement.GetEndpointInfo(),
                                service_id,
                                BLUETOOTH,
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
  LOG(INFO) << "Ble: [LOST, SCHED] peripheral_name=" << peripheral_name;
  RunOnPcpHandlerThread(
      "p2p-ble-device-lost",
      [this, client, service_id, &peripheral]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering() || stop_.Get()) {
          LOG(WARNING) << "Ignoring lost BlePeripheral  because we are "
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
          VLOG(1) << "Lost BleEndpoint for BlePeripheral "
                  << peripheral.GetName()
                  << " (with endpoint_id=" << ble_endpoint_state.endpoint_id
                  << " and endpoint_info="
                  << absl::BytesToHexString(
                         ble_endpoint_state.endpoint_info.data())
                  << ").";
          OnEndpointLost(client, DiscoveredEndpoint{
                                     ble_endpoint_state.endpoint_id,
                                     ble_endpoint_state.endpoint_info,
                                     service_id,
                                     BLE,
                                     WebRtcState::kUndefined,
                                 });
        }
      });
}

bool P2pClusterPcpHandler::IsRecognizedBleV2Endpoint(
    absl::string_view service_id, const BleAdvertisement& advertisement) const {
  if (!advertisement.IsValid()) {
    LOG(INFO) << "BleAdvertisement doesn't conform to the format, discarding.";
    return false;
  }

  if (advertisement.GetVersion() != kBleAdvertisementVersion) {
    LOG(INFO) << "BleAdvertisement has an unknown version; expected "
              << static_cast<int>(kBleAdvertisementVersion) << ", found "
              << static_cast<int>(advertisement.GetVersion());
    return false;
  }

  if (advertisement.GetPcp() != GetPcp()) {
    LOG(INFO) << "BleAdvertisement doesn't match on Pcp; expected "
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
      LOG(INFO)
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
  RunOnPcpHandlerThread(
      "p2p-ble-peripheral-discovered",
      [this, client, peripheral = std::move(peripheral), service_id,
       advertisement_bytes, fast_advertisement]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering() || stop_.Get()) {
          LOG(WARNING) << "Skipping discovery of BleAdvertisement header "
                       << absl::BytesToHexString(
                              advertisement_bytes.AsStringView())
                       << " because we are no longer discovering.";
          return;
        }

        if (client->GetDiscoveryOptions()
                .fast_advertisement_service_uuid.empty() &&
            fast_advertisement) {
          LOG(INFO) << "Ignore the fast advertisement due to cient "
                       "doesn't receive it.";
          return;
        }

        auto ble_status_or = BleAdvertisement::CreateBleAdvertisement(
            fast_advertisement, advertisement_bytes);
        if (!ble_status_or.ok()) {
          LOG(ERROR) << ble_status_or.status();
          return;
        }
        const auto& advertisement = ble_status_or.value();

        // Make sure the BLE advertisement points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedBleV2Endpoint(service_id, advertisement)) {
          LOG(ERROR) << "IsRecognizedBleV2Endpoint failed";
          return;
        }

        // Report the discovered endpoint to the client.
        BleV2EndpointState ble_endpoint_state;
        ByteArray peripheral_id = peripheral.GetId();
        found_endpoints_in_ble_discover_cb_.insert(
            {peripheral_id, ble_endpoint_state});

        ble_endpoint_state.ble = true;
        found_endpoints_in_ble_discover_cb_[peripheral_id] = ble_endpoint_state;
        VLOG(1) << "Found " << advertisement.ToReadableString();
        StopEndpointLostByMediumAlarm(advertisement.GetEndpointId(), BLE);
        OnEndpointFound(
            client,
            std::make_shared<BleV2Endpoint>(BleV2Endpoint{
                {advertisement.GetEndpointId(), advertisement.GetEndpointInfo(),
                 service_id, BLE, advertisement.GetWebRtcState()},
                std::move(peripheral),
            }));

        // Make sure we can connect to this device via Classic Bluetooth.
        MacAddress remote_bluetooth_mac_address =
            advertisement.GetBluetoothMacAddress();
        if (!remote_bluetooth_mac_address.IsSet()) {
          LOG(INFO)
              << "No Bluetooth Classic MAC address found in advertisement.";
          return;
        }

        BluetoothDevice remote_bluetooth_device =
            bluetooth_medium_.GetRemoteDevice(remote_bluetooth_mac_address);
        if (!remote_bluetooth_device.IsValid()) {
          LOG(ERROR)
              << "A valid Bluetooth device could not be derived from the MAC "
                 "address "
              << remote_bluetooth_mac_address.ToString();
          return;
        }

        ble_endpoint_state.bt = true;
        found_endpoints_in_ble_discover_cb_[peripheral_id] = ble_endpoint_state;
        StopEndpointLostByMediumAlarm(advertisement.GetEndpointId(), BLUETOOTH);

        OnEndpointFound(client,
                        std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
                            {
                                advertisement.GetEndpointId(),
                                advertisement.GetEndpointInfo(),
                                service_id,
                                BLUETOOTH,
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
          LOG(WARNING) << "Ignoring lost BlePeripheral "
                       << absl::BytesToHexString(peripheral.GetId().data())
                       << " because we are no longer discovering.";
          return;
        }

        auto ble_status_or = BleAdvertisement::CreateBleAdvertisement(
            fast_advertisement, advertisement_bytes);
        if (!ble_status_or.ok()) {
          LOG(ERROR) << ble_status_or.status();
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
          VLOG(1) << "Lost BleEndpoint for BlePeripheral "
                  << absl::BytesToHexString(peripheral.GetId().data())
                  << " (with endpoint_id=" << advertisement.GetEndpointId()
                  << " and endpoint_info="
                  << absl::BytesToHexString(
                         advertisement.GetEndpointInfo().data())
                  << ").";
          OnEndpointLost(client, DiscoveredEndpoint{
                                     advertisement.GetEndpointId(),
                                     advertisement.GetEndpointInfo(),
                                     service_id,
                                     BLE,
                                     WebRtcState::kUndefined,
                                 });
        }
        if (ble_endpoint_state.bt) {
          // Report the lost endpoint to the client.
          VLOG(1) << "Lost BluetoothEndpoint for BlePeripheral "
                  << absl::BytesToHexString(peripheral.GetId().data())
                  << " (with endpoint_id=" << advertisement.GetEndpointId()
                  << " and endpoint_info="
                  << absl::BytesToHexString(
                         advertisement.GetEndpointInfo().data())
                  << ").";
          OnEndpointLost(client, DiscoveredEndpoint{
                                     advertisement.GetEndpointId(),
                                     advertisement.GetEndpointInfo(),
                                     service_id,
                                     BLUETOOTH,
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
          LOG(WARNING) << "Ignoring instant lost BlePeripheral "
                       << absl::BytesToHexString(peripheral.GetId().data())
                       << " because we are no longer discovering.";
          return;
        }

        LOG(INFO) << "Processing instant lost on BlePeripheral "
                  << absl::BytesToHexString(peripheral.GetId().data());
        auto ble_status_or = BleAdvertisement::CreateBleAdvertisement(
            fast_advertisement, advertisement_bytes);
        if (!ble_status_or.ok()) {
          LOG(ERROR) << ble_status_or.status();
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

        LOG(INFO) << "Found nearby legacy BLE device, pending "
                     "bluetooth discovery size :"
                  << paused_bluetooth_clients_discoveries_.size();

        for (auto& paused_bluetooth_client :
             paused_bluetooth_clients_discoveries_) {
          if (!paused_bluetooth_client.second->IsDiscoveringServiceId(
                  paused_bluetooth_client.first)) {
            LOG(INFO) << "Do not start bluetooth scanning since client "
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

void P2pClusterPcpHandler::AwdlServiceDiscoveredHandler(
    ClientProxy* client, NsdServiceInfo service_info,
    const std::string& service_id) {
  RunOnPcpHandlerThread(
      "p2p-awdl-service-discovered",
      [this, client, service_id, service_info]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering()) {
          LOG(WARNING) << "Skipping discovery of NsdServiceInfo "
                       << service_info.GetServiceName()
                       << " because we are no longer discovering.";
          return;
        }

        // Parse the WifiLanServiceInfo.
        WifiLanServiceInfo wifi_lan_service_info(service_info);
        // Make sure the medium service name points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedWifiServiceEndpoint(service_id,
                                             wifi_lan_service_info)) {
          return;
        }

        // Report the discovered endpoint to the client.
        VLOG(1) << "Found NsdServiceInfo "
                << "with (service_name:" << service_info.GetServiceName()
                << ", service_type:" << service_info.GetServiceType()
                << ", endpoint_id:" << wifi_lan_service_info.GetEndpointId()
                << ", endpoint_info:"
                << absl::BytesToHexString(
                       wifi_lan_service_info.GetEndpointInfo().AsStringView())
                << ").";
        StopEndpointLostByMediumAlarm(wifi_lan_service_info.GetEndpointId(),
                                      AWDL);
        OnEndpointFound(client, std::make_shared<AwdlEndpoint>(AwdlEndpoint{
                                    {
                                        wifi_lan_service_info.GetEndpointId(),
                                        wifi_lan_service_info.GetEndpointInfo(),
                                        service_id,
                                        AWDL,
                                        wifi_lan_service_info.GetWebRtcState(),
                                    },
                                    service_info,
                                }));
      });
}

void P2pClusterPcpHandler::AwdlServiceLostHandler(
    ClientProxy* client, NsdServiceInfo service_info,
    const std::string& service_id) {
  LOG(INFO) << "Awdl: [LOST, SCHED] service_info=" << &service_info
            << ", service_name=" << service_info.GetServiceName();
  RunOnPcpHandlerThread(
      "p2p-wifi-service-lost",
      [this, client, service_id, service_info]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering()) {
          LOG(WARNING) << "Ignoring lost NsdServiceInfo "
                       << service_info.GetServiceName()
                       << " because we are no longer "
                          "discovering.";
          return;
        }

        // Parse the WifiLanServiceInfo.
        WifiLanServiceInfo wifi_lan_service_info(service_info);

        // Make sure the medium service name points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedWifiServiceEndpoint(service_id, wifi_lan_service_info))
          return;

        // Report the lost endpoint to the client.
        VLOG(1) << "Lost NsdServiceInfo " << service_info.GetServiceName()
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
                                   AWDL,
                                   WebRtcState::kUndefined,
                               });
      });
}

bool P2pClusterPcpHandler::IsRecognizedWifiServiceEndpoint(
    const std::string& service_id,
    const WifiLanServiceInfo& wifi_lan_service_info) const {
  if (!wifi_lan_service_info.IsValid()) {
    LOG(INFO)
        << "WifiLanServiceInfo doesn't conform to the format, discarding.";
    return false;
  }

  if (wifi_lan_service_info.GetPcp() != GetPcp()) {
    LOG(INFO) << "WifiLanServiceInfo doesn't match on Pcp; expected "
              << PcpToStrategy(GetPcp()).GetName() << ", found "
              << PcpToStrategy(wifi_lan_service_info.GetPcp()).GetName();
    return false;
  }

  ByteArray expected_service_id_hash =
      GenerateHash(service_id, WifiLanServiceInfo::kServiceIdHashLength);

  if (wifi_lan_service_info.GetServiceIdHash() != expected_service_id_hash) {
    LOG(INFO)
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
          LOG(WARNING) << "Skipping discovery of NsdServiceInfo "
                       << service_info.GetServiceName()
                       << " because we are no longer discovering.";
          return;
        }

        // Parse the WifiLanServiceInfo.
        WifiLanServiceInfo wifi_lan_service_info(service_info);
        // Make sure the WifiLan service name points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedWifiServiceEndpoint(service_id,
                                             wifi_lan_service_info)) {
          return;
        }

        // Report the discovered endpoint to the client.
        VLOG(1) << "Found NsdServiceInfo " << service_info.GetServiceName()
                << " (with endpoint_id="
                << wifi_lan_service_info.GetEndpointId() << "and endpoint_info="
                << absl::BytesToHexString(
                       wifi_lan_service_info.GetEndpointInfo().data())
                << ").";
        StopEndpointLostByMediumAlarm(wifi_lan_service_info.GetEndpointId(),
                                      WIFI_LAN);
        OnEndpointFound(client,
                        std::make_shared<WifiLanEndpoint>(WifiLanEndpoint{
                            {
                                wifi_lan_service_info.GetEndpointId(),
                                wifi_lan_service_info.GetEndpointInfo(),
                                service_id,
                                WIFI_LAN,
                                wifi_lan_service_info.GetWebRtcState(),
                            },
                            service_info,
                        }));
      });
}

void P2pClusterPcpHandler::WifiLanServiceLostHandler(
    ClientProxy* client, NsdServiceInfo service_info,
    const std::string& service_id) {
  LOG(INFO) << "WifiLan: [LOST, SCHED] service_info=" << &service_info
            << ", service_name=" << service_info.GetServiceName();
  RunOnPcpHandlerThread(
      "p2p-wifi-service-lost",
      [this, client, service_id, service_info]() RUN_ON_PCP_HANDLER_THREAD() {
        // Make sure we are still discovering before proceeding.
        if (!client->IsDiscovering()) {
          LOG(WARNING) << "Ignoring lost NsdServiceInfo "
                       << service_info.GetServiceName()
                       << " because we are no longer "
                          "discovering.";
          return;
        }

        // Parse the WifiLanServiceInfo.
        WifiLanServiceInfo wifi_lan_service_info(service_info);

        // Make sure the WifiLan service name points to a valid
        // endpoint we're discovering.
        if (!IsRecognizedWifiServiceEndpoint(service_id, wifi_lan_service_info))
          return;

        // Report the lost endpoint to the client.
        VLOG(1) << "Lost NsdServiceInfo " << service_info.GetServiceName()
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
                                   WIFI_LAN,
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
  std::vector<ConnectionsLog::OperationResultWithMedium>
      operation_result_with_mediums;

  // Due to singleton, apple only allow start discovery once. So need to keep
  // the start discovery order of awdl before the wifi_lan.
  if (discovery_options.allowed.awdl &&
      NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableAwdl)) {
    ErrorOr<Medium> awdl_result = StartAwdlDiscovery(client, service_id);
    Medium awdl_medium = UNKNOWN_MEDIUM;
    if (awdl_result.has_value()) {
      awdl_medium = awdl_result.value();
    }
    if (awdl_medium != UNKNOWN_MEDIUM) {
      LOG(INFO) << "P2pClusterPcpHandler::StartDiscoveryImpl: AWDL added";
      mediums_started_successfully.push_back(awdl_medium);
    }
    std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
        operation_result_with_medium = GetOperationResultWithMediumByResultCode(
            client, AWDL,
            /*update_index=*/0,
            awdl_result.has_error()
                ? awdl_result.error().operation_result_code().value()
                : OperationResultCode::DETAIL_SUCCESS);
    operation_result_with_mediums.push_back(*operation_result_with_medium);
  }

  // WifiLan
  if (discovery_options.allowed.wifi_lan) {
    ErrorOr<Medium> wifi_lan_result = StartWifiLanDiscovery(client, service_id);
    Medium wifi_lan_medium = UNKNOWN_MEDIUM;
    if (wifi_lan_result.has_value()) {
      wifi_lan_medium = wifi_lan_result.value();
    }
    if (wifi_lan_medium != UNKNOWN_MEDIUM) {
      LOG(INFO) << "P2pClusterPcpHandler::StartDiscoveryImpl: WifiLan added";
      mediums_started_successfully.push_back(wifi_lan_medium);
    }
    std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
        operation_result_with_medium = GetOperationResultWithMediumByResultCode(
            client, WIFI_LAN,
            /*update_index=*/0,
            wifi_lan_result.has_error()
                ? wifi_lan_result.error().operation_result_code().value()
                : OperationResultCode::DETAIL_SUCCESS);
    operation_result_with_mediums.push_back(*operation_result_with_medium);
  }

  if (discovery_options.allowed.ble) {
    ErrorOr<Medium> ble_result = {Error(OperationResultCode::DETAIL_UNKNOWN)};
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
      ble_result = StartBleV2Scanning(client, service_id, discovery_options);
      Medium ble_v2_medium = UNKNOWN_MEDIUM;
      if (ble_result.has_value()) {
        ble_v2_medium = ble_result.value();
      }
      if (ble_v2_medium != UNKNOWN_MEDIUM) {
        LOG(INFO) << "P2pClusterPcpHandler::StartDiscoveryImpl: Ble v2 added";
        mediums_started_successfully.push_back(ble_v2_medium);
      }
    } else {
      ble_result =
          StartBleScanning(client, service_id,
                           discovery_options.fast_advertisement_service_uuid);
      Medium ble_medium = UNKNOWN_MEDIUM;
      if (ble_result.has_value()) {
        ble_medium = ble_result.value();
      }
      if (ble_medium != UNKNOWN_MEDIUM) {
        LOG(INFO) << "P2pClusterPcpHandler::StartDiscoveryImpl: Ble added";
        mediums_started_successfully.push_back(ble_medium);
      }
    }
    std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
        operation_result_with_medium = GetOperationResultWithMediumByResultCode(
            client, BLE,
            /*update_index=*/0,
            ble_result.has_error()
                ? ble_result.error().operation_result_code().value()
                : OperationResultCode::DETAIL_SUCCESS);
    operation_result_with_mediums.push_back(*operation_result_with_medium);
  }

  if (discovery_options.allowed.bluetooth) {
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kDisableBluetoothClassicScanning)) {
      StartBluetoothDiscoveryWithPause(
          client, service_id, discovery_options, mediums_started_successfully,
          operation_result_with_mediums, /*update_index=*/0);
    } else {
      ErrorOr<Medium> bluetooth_result =
          StartBluetoothDiscovery(client, service_id);
      if (bluetooth_result.has_value()) {
        LOG(INFO) << "P2pClusterPcpHandler::StartDiscoveryImpl: BT added";
        mediums_started_successfully.push_back(*bluetooth_result);
        bluetooth_classic_client_id_to_service_id_map_.insert(
            {client->GetClientId(), service_id});
      }
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, BLUETOOTH,
                  /*update_index=*/0,
                  bluetooth_result.has_error()
                      ? bluetooth_result.error().operation_result_code().value()
                      : OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    }
  }

  if (mediums_started_successfully.empty()) {
    LOG(ERROR) << "Failed StartDiscovery() for client=" << client->GetClientId()
               << " because we couldn't scan on Bluetooth, BLE, or WifiLan for "
                  "service_id="
               << service_id;
    // TODO(edwinwu): Modify the status instead of kBluetoothError
    return {
        .status = {Status::kBluetoothError},
        .operation_result_with_mediums =
            std::move(operation_result_with_mediums),
    };
  }

  return {
      .status = {Status::kSuccess},
      .mediums = std::move(mediums_started_successfully),
      .operation_result_with_mediums = std::move(operation_result_with_mediums),
  };
}

Status P2pClusterPcpHandler::StopDiscoveryImpl(ClientProxy* client) {
  wifi_lan_medium_.StopDiscovery(client->GetDiscoveryServiceId());
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableAwdl)) {
    awdl_medium_.StopDiscovery(client->GetDiscoveryServiceId());
  }

  if (bluetooth_classic_client_id_to_service_id_map_.contains(
          client->GetClientId())) {
    bluetooth_medium_.StopDiscovery(
        bluetooth_classic_client_id_to_service_id_map_.at(
            client->GetClientId()));
    bluetooth_classic_client_id_to_service_id_map_.erase(client->GetClientId());
  } else {
    LOG(INFO) << "Skipped BT StopDiscovery for client=" << client->GetClientId()
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
  LOG(INFO) << "InjectEndpoint.";
  // Bluetooth is the only supported out-of-band connection medium.
  if (metadata.medium != BLUETOOTH) {
    LOG(WARNING) << "InjectEndpointImpl: Only Bluetooth is supported.";
    return {Status::kError};
  }

  // Make sure discovery is in out-of-band  mode from the API definition in
  // core.h.
  if (!client->GetDiscoveryOptions().is_out_of_band_connection) {
    LOG(WARNING) << "InjectEndpointImpl: Discovery is not in out-of-band mode.";
    return {Status::kError};
  }

  BluetoothDevice remote_bluetooth_device =
      injected_bluetooth_device_store_.CreateInjectedBluetoothDevice(
          metadata.remote_bluetooth_mac_address, metadata.endpoint_id,
          metadata.endpoint_info,
          GenerateHash(service_id, BluetoothDeviceName::kServiceIdHashLength),
          GetPcp());

  if (!remote_bluetooth_device.IsValid()) {
    LOG(WARNING) << "InjectEndpointImpl: Invalid parameters.";
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
    case BLUETOOTH: {
      auto* bluetooth_endpoint = down_cast<BluetoothEndpoint*>(endpoint);
      if (bluetooth_endpoint) {
        return BluetoothConnectImpl(client, bluetooth_endpoint);
      }
      break;
    }
    case BLE: {
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
    case WIFI_LAN: {
      auto* wifi_lan_endpoint = down_cast<WifiLanEndpoint*>(endpoint);
      if (wifi_lan_endpoint) {
        return WifiLanConnectImpl(client, wifi_lan_endpoint);
      }
      break;
    }
    case AWDL: {
      auto* awdl_endpoint = down_cast<AwdlEndpoint*>(endpoint);
      if (awdl_endpoint) {
        return AwdlConnectImpl(client, awdl_endpoint);
      }
      break;
    }
    case WEB_RTC: {
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
  std::vector<ConnectionsLog::OperationResultWithMedium>
      operation_result_with_mediums;
  int update_index =
      client_proxy->GetAnalyticsRecorder().GetNextAdvertisingUpdateIndex();
  if (options.enable_bluetooth_listening &&
      !bluetooth_medium_.IsAcceptingConnections(std::string(service_id))) {
    ErrorOr<bool> bluetooth_result =
        bluetooth_medium_.StartAcceptingConnections(
            std::string(service_id),
            absl::bind_front(
                &P2pClusterPcpHandler::BluetoothConnectionAcceptedHandler, this,
                client_proxy, local_endpoint_id,
                options.listening_endpoint_type));
    if (bluetooth_result.has_error()) {
      LOG(WARNING)
          << "Failed to start listening for incoming connections on Bluetooth";
    } else {
      started_mediums.push_back(BLUETOOTH);
    }
    std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
        operation_result_with_medium = GetOperationResultWithMediumByResultCode(
            client_proxy, Medium::BLUETOOTH, update_index,
            bluetooth_result.has_error()
                ? bluetooth_result.error().operation_result_code().value()
                : OperationResultCode::DETAIL_SUCCESS);
    operation_result_with_mediums.push_back(*operation_result_with_medium);
  }
  // ble
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
    // ble_v2
    // TODO(mingshiouwu): Add unit test for ble_l2cap flow
    bool accepting_ble_connections_success = false;
    if (options.enable_ble_listening &&
        NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kEnableBleL2cap) &&
        !ble_v2_medium_.IsAcceptingL2capConnections(std::string(service_id))) {
      if (!ble_v2_medium_.StartAcceptingL2capConnections(
              std::string(service_id),
              absl::bind_front(
                  &P2pClusterPcpHandler::BleL2capConnectionAcceptedHandler,
                  this, client_proxy, local_endpoint_id,
                  options.listening_endpoint_type))) {
        LOG(WARNING) << "Failed to start listening for incoming L2CAP "
                        "connections on ble_v2";
      } else {
        accepting_ble_connections_success = true;
      }
    }

    if (options.enable_ble_listening &&
        !ble_v2_medium_.IsAcceptingConnections(std::string(service_id))) {
      if (!ble_v2_medium_.StartAcceptingConnections(
              std::string(service_id),
              absl::bind_front(
                  &P2pClusterPcpHandler::BleV2ConnectionAcceptedHandler, this,
                  client_proxy, local_endpoint_id,
                  options.listening_endpoint_type))) {
        LOG(WARNING)
            << "Failed to start listening for incoming connections on ble_v2";
      } else {
        accepting_ble_connections_success = true;
      }
    }
    if (accepting_ble_connections_success) {
      started_mediums.push_back(BLE);
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
        LOG(WARNING)
            << "Failed to start listening for incoming connections on ble";
      } else {
        started_mediums.push_back(BLE);
      }
    }
  }
  // wifi lan
  if (options.enable_wlan_listening &&
      !wifi_lan_medium_.IsAcceptingConnections(std::string(service_id))) {
    ErrorOr<bool> wifi_lan_result = wifi_lan_medium_.StartAcceptingConnections(
        std::string(service_id),
        absl::bind_front(
            &P2pClusterPcpHandler::WifiLanConnectionAcceptedHandler, this,
            client_proxy, local_endpoint_id, "",
            options.listening_endpoint_type));
    if (wifi_lan_result.has_error()) {
      LOG(WARNING)
          << "Failed to start listening for incoming connections on wifi_lan";
    } else {
      started_mediums.push_back(WIFI_LAN);
    }
    std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
        operation_result_with_medium = GetOperationResultWithMediumByResultCode(
            client_proxy, Medium::BLUETOOTH, update_index,
            wifi_lan_result.has_error()
                ? wifi_lan_result.error().operation_result_code().value()
                : OperationResultCode::DETAIL_SUCCESS);
    operation_result_with_mediums.push_back(*operation_result_with_medium);
  }
  if (started_mediums.empty()) {
    LOG(WARNING) << absl::StrFormat(
        "Failed StartListeningForIncomingConnectionsImpl() for client %d for "
        "service_id %s",
        client_proxy->GetClientId(), service_id);
    return StartOperationResult{
        .status = {Status::kError},
        .operation_result_with_mediums =
            std::move(operation_result_with_mediums),
    };
  }
  return BasePcpHandler::StartOperationResult{
      .status = {Status::kSuccess},
      .mediums = std::move(started_mediums),
      .operation_result_with_mediums = std::move(operation_result_with_mediums),
  };
}

void P2pClusterPcpHandler::StopListeningForIncomingConnectionsImpl(
    ClientProxy* client) {
  if (wifi_lan_medium_.IsAcceptingConnections(
          client->GetListeningForIncomingConnectionsServiceId())) {
    if (!wifi_lan_medium_.StopAcceptingConnections(
            client->GetListeningForIncomingConnectionsServiceId())) {
      LOG(WARNING) << "Unable to stop wifi lan from accepting connections.";
    }
  }
  if (bluetooth_medium_.IsAcceptingConnections(
          client->GetListeningForIncomingConnectionsServiceId())) {
    if (!bluetooth_medium_.StopAcceptingConnections(
            client->GetListeningForIncomingConnectionsServiceId())) {
      LOG(WARNING)
          << "Unable to stop bluetooth medium from accepting connections.";
    }
  }
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
    if (ble_v2_medium_.IsAcceptingConnections(
            client->GetListeningForIncomingConnectionsServiceId())) {
      if (!ble_v2_medium_.StopAcceptingConnections(
              client->GetListeningForIncomingConnectionsServiceId())) {
        LOG(WARNING)
            << "Unable to stop ble_v2 medium from accepting connections.";
      }
    }
  } else {
    if (ble_medium_.IsAcceptingConnections(
            client->GetListeningForIncomingConnectionsServiceId())) {
      if (!ble_medium_.StopAcceptingConnections(
              client->GetListeningForIncomingConnectionsServiceId())) {
        LOG(WARNING) << "Unable to stop ble medium from accepting connections.";
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
  if (NeedsToTurnOffAdvertisingMedium(BLE, old_options, advertising_options) ||
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
  // awdl
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableAwdl) &&
      (NeedsToTurnOffAdvertisingMedium(AWDL, old_options,
                                       advertising_options) ||
       needs_restart)) {
    mediums_->GetAwdl().StopAdvertising(std::string(service_id));
    mediums_->GetAwdl().StopAcceptingConnections(std::string(service_id));
  }
  // wifi lan
  if (NeedsToTurnOffAdvertisingMedium(WIFI_LAN, old_options,
                                      advertising_options) ||
      needs_restart) {
    mediums_->GetWifiLan().StopAdvertising(std::string(service_id));
    mediums_->GetWifiLan().StopAcceptingConnections(std::string(service_id));
  }
  // Bluetooth classic
  if (NeedsToTurnOffAdvertisingMedium(BLUETOOTH, old_options,
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
  std::vector<ConnectionsLog::OperationResultWithMedium>
      operation_result_with_mediums;
  int update_index =
      client->GetAnalyticsRecorder().GetNextAdvertisingUpdateIndex();
  Status status = {Status::kSuccess};
  WebRtcState web_rtc_state = webrtc_medium_.IsAvailable()
                                  ? WebRtcState::kConnectable
                                  : WebRtcState::kUndefined;
  // ble
  auto new_mediums = advertising_options.allowed;
  auto old_mediums = old_options.allowed;
  if (new_mediums.ble) {
    if (old_mediums.ble && !needs_restart) {
      restarted_mediums.push_back(BLE);
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, BLE, update_index,
                  OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    } else {
      ErrorOr<Medium> ble_result = {Error(OperationResultCode::DETAIL_UNKNOWN)};
      if (NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_connections_feature::
                  kEnableBleV2)) {
        ble_result = StartBleV2Advertising(
            client, std::string(service_id), std::string(local_endpoint_id),
            ByteArray(std::string(local_endpoint_info)), advertising_options,
            web_rtc_state);
        if (ble_result.has_value() && ble_result.value() != UNKNOWN_MEDIUM) {
          restarted_mediums.push_back(BLE);
        } else {
          status = {Status::kBleError};
        }
      } else {
        ble_result = StartBleAdvertising(
            client, std::string(service_id), std::string(local_endpoint_id),
            ByteArray(std::string(local_endpoint_info)), advertising_options,
            web_rtc_state);
        if (ble_result.has_value() && ble_result.value() != UNKNOWN_MEDIUM) {
          restarted_mediums.push_back(BLE);
        } else {
          status = {Status::kBleError};
        }
      }
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, BLE, update_index,
                  ble_result.has_error()
                      ? ble_result.error().operation_result_code().value()
                      : OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    }
  }
  // awdl
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableAwdl) &&
      new_mediums.awdl && !advertising_options.low_power) {
    if (old_mediums.awdl && !needs_restart) {
      restarted_mediums.push_back(AWDL);
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, AWDL, update_index,
                  OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    } else {
      ErrorOr<Medium> awdl_result = StartAwdlAdvertising(
          client, std::string(service_id), std::string(local_endpoint_id),
          ByteArray(std::string(local_endpoint_info)), web_rtc_state);
      if (awdl_result.has_value() && awdl_result.value() != UNKNOWN_MEDIUM) {
        restarted_mediums.push_back(AWDL);
      } else {
        status = {Status::kWifiLanError};
      }
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, AWDL, update_index,
                  awdl_result.has_error()
                      ? awdl_result.error().operation_result_code().value()
                      : OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    }
  }
  // wifi lan
  if (new_mediums.wifi_lan && !advertising_options.low_power) {
    if (old_mediums.wifi_lan && !needs_restart) {
      restarted_mediums.push_back(WIFI_LAN);
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, WIFI_LAN, update_index,
                  OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    } else {
      ErrorOr<Medium> wifi_lan_result = StartWifiLanAdvertising(
          client, std::string(service_id), std::string(local_endpoint_id),
          ByteArray(std::string(local_endpoint_info)), web_rtc_state);
      if (wifi_lan_result.has_value() &&
          wifi_lan_result.value() != UNKNOWN_MEDIUM) {
        restarted_mediums.push_back(WIFI_LAN);
      } else {
        status = {Status::kWifiLanError};
      }
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, WIFI_LAN, update_index,
                  wifi_lan_result.has_error()
                      ? wifi_lan_result.error().operation_result_code().value()
                      : OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    }
  }
  // bluetooth classic
  if (new_mediums.bluetooth && !advertising_options.low_power) {
    if (old_mediums.bluetooth && !needs_restart) {
      restarted_mediums.push_back(BLUETOOTH);
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, BLUETOOTH, update_index,
                  OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    } else {
      const ByteArray bluetooth_hash = GenerateHash(
          std::string(service_id), BluetoothDeviceName::kServiceIdHashLength);
      ErrorOr<Medium> bluetooth_result = StartBluetoothAdvertising(
          client, std::string(service_id), bluetooth_hash,
          std::string(local_endpoint_id),
          ByteArray(std::string(local_endpoint_info)), web_rtc_state);
      if (bluetooth_result.has_value() &&
          bluetooth_result.value() != UNKNOWN_MEDIUM) {
        // TODO(hais): update this after ble_v2 refactor.
        if (api::ImplementationPlatform::GetCurrentOS() ==
                api::OSName::kChromeOS &&
            !NearbyFlags::GetInstance().GetBoolFlag(
                config_package_nearby::nearby_connections_feature::
                    kEnableBleV2)) {
          if (ble_medium_.StartLegacyAdvertising(
                  std::string(service_id), std::string(local_endpoint_id),
                  advertising_options.fast_advertisement_service_uuid)) {
            LOG(INFO) << "P2pClusterPcpHandler::UpdateAdvertisingOptionsImpl: "
                         "Ble legacy started advertising";
            LOG(INFO) << "P2pClusterPcpHandler::"
                         "UpdateAdvertisingOptionsImpl: BT added";
            restarted_mediums.push_back(BLUETOOTH);
          } else {
            LOG(WARNING)
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
            LOG(INFO) << __func__
                      << "Ble v2 started advertising for legacy device.";
            restarted_mediums.push_back(BLUETOOTH);
            LOG(INFO) << __func__
                      << "After Ble v2 started advertising, for "
                         "legacy, BT added to restarted mediums";
          } else {
            LOG(WARNING)
                << __func__
                << "BLE v2 failed advertising for legacy device, revert BTC";
            bluetooth_medium_.TurnOffDiscoverability();
            bluetooth_medium_.StopAcceptingConnections(std::string(service_id));
          }

        } else {
          restarted_mediums.push_back(BLUETOOTH);
        }
        std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
            operation_result_with_medium =
                GetOperationResultWithMediumByResultCode(
                    client, BLUETOOTH, update_index,
                    bluetooth_result.has_error()
                        ? bluetooth_result.error()
                              .operation_result_code()
                              .value()
                        : OperationResultCode::DETAIL_SUCCESS);
        operation_result_with_mediums.push_back(*operation_result_with_medium);
      } else {
        std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
            operation_result_with_medium =
                GetOperationResultWithMediumByResultCode(
                    client, BLUETOOTH, update_index,
                    bluetooth_result.has_error()
                        ? bluetooth_result.error()
                              .operation_result_code()
                              .value()
                        : OperationResultCode::DETAIL_SUCCESS);
        operation_result_with_mediums.push_back(*operation_result_with_medium);
        return StartOperationResult{.status = {Status::kBluetoothError},
                                    .mediums = restarted_mediums,
                                    .operation_result_with_mediums = std::move(
                                        operation_result_with_mediums)};
      }
    }
  }
  return StartOperationResult{.status = status,
                              .mediums = restarted_mediums,
                              .operation_result_with_mediums =
                                  std::move(operation_result_with_mediums)};
}

BasePcpHandler::StartOperationResult
P2pClusterPcpHandler::UpdateDiscoveryOptionsImpl(
    ClientProxy* client, absl::string_view service_id,
    absl::string_view local_endpoint_id, absl::string_view local_endpoint_info,
    const DiscoveryOptions& discovery_options) {
  DiscoveryOptions old_options = client->GetDiscoveryOptions();
  bool needs_restart = old_options.low_power != discovery_options.low_power;
  // ble
  if (NeedsToTurnOffDiscoveryMedium(BLE, old_options, discovery_options)) {
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::kEnableBleV2)) {
      ble_v2_medium_.StopScanning(std::string(service_id));
    } else {
      ble_medium_.StopScanning(std::string(service_id));
    }
    StartEndpointLostByMediumAlarms(client, BLE);
  }
  // bt classic
  if (NeedsToTurnOffDiscoveryMedium(BLUETOOTH, old_options,
                                    discovery_options) ||
      needs_restart) {
    bluetooth_medium_.StopDiscovery(std::string(service_id));
    StartEndpointLostByMediumAlarms(client, BLUETOOTH);
  }
  // awdl
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableAwdl) &&
      (NeedsToTurnOffDiscoveryMedium(AWDL, old_options, discovery_options) ||
       needs_restart)) {
    mediums_->GetAwdl().StopDiscovery(std::string(service_id));
    StartEndpointLostByMediumAlarms(client, AWDL);
  }
  // wifi lan
  if (NeedsToTurnOffDiscoveryMedium(WIFI_LAN, old_options, discovery_options) ||
      needs_restart) {
    mediums_->GetWifiLan().StopDiscovery(std::string(service_id));
    StartEndpointLostByMediumAlarms(client, WIFI_LAN);
  }
  // restart
  std::vector<Medium> restarted_mediums;
  bool should_start_discovery = false;
  auto new_mediums = discovery_options.allowed;
  auto old_mediums = old_options.allowed;
  std::vector<ConnectionsLog::OperationResultWithMedium>
      operation_result_with_mediums;
  int update_index =
      client->GetAnalyticsRecorder().GetNextDiscoveryUpdateIndex();
  // ble
  if (new_mediums.ble) {
    should_start_discovery = true;
    if (old_mediums.ble) {
      restarted_mediums.push_back(BLE);
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, BLE, update_index,
                  OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    } else {
      ErrorOr<Medium> ble_result = {Error(OperationResultCode::DETAIL_UNKNOWN)};
      if (NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_connections_feature::
                  kEnableBleV2)) {
        ble_result = StartBleV2Scanning(client, std::string(service_id),
                                        discovery_options);
        if (ble_result.has_value()) {
          restarted_mediums.push_back(BLE);
        } else {
          LOG(WARNING) << "UpdateDiscoveryOptionsImpl: unable to "
                          "restart blev2 scanning";
        }
      } else {
        ble_result =
            StartBleScanning(client, std::string(service_id),
                             discovery_options.fast_advertisement_service_uuid);
        if (ble_result.has_value()) {
          restarted_mediums.push_back(BLE);
        } else {
          LOG(WARNING)
              << "UpdateDiscoveryOptionsImpl: unable to restart ble scanning";
        }
      }
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, BLE, update_index,
                  ble_result.has_error()
                      ? ble_result.error().operation_result_code().value()
                      : OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    }
  }
  // bt classic
  if (new_mediums.bluetooth && !discovery_options.low_power) {
    should_start_discovery = true;
    if (!needs_restart && old_mediums.bluetooth) {
      restarted_mediums.push_back(BLUETOOTH);
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, BLUETOOTH, update_index,
                  OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    } else {
      if (NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_connections_feature::
                  kDisableBluetoothClassicScanning)) {
        StartBluetoothDiscoveryWithPause(
            client, std::string(service_id), discovery_options,
            restarted_mediums, operation_result_with_mediums, update_index);
      } else {
        ErrorOr<Medium> bluetooth_result =
            StartBluetoothDiscovery(client, std::string(service_id));
        if (bluetooth_result.has_value()) {
          restarted_mediums.push_back(BLUETOOTH);
        } else {
          LOG(WARNING)
              << "UpdateDiscoveryOptionsImpl: unable to restart bt scanning";
        }
        std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
            operation_result_with_medium =
                GetOperationResultWithMediumByResultCode(
                    client, BLUETOOTH, update_index,
                    bluetooth_result.has_error()
                        ? bluetooth_result.error()
                              .operation_result_code()
                              .value()
                        : OperationResultCode::DETAIL_SUCCESS);
        operation_result_with_mediums.push_back(*operation_result_with_medium);
      }
    }
  }
  // awdl (note: keep the awdl logic before the wifi lan logic)
  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableAwdl) &&
      new_mediums.awdl && !discovery_options.low_power) {
    should_start_discovery = true;
    if (!needs_restart && old_mediums.awdl) {
      restarted_mediums.push_back(AWDL);
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, AWDL, update_index,
                  OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    } else {
      ErrorOr<Medium> awdl_result =
          StartAwdlDiscovery(client, std::string(service_id));
      if (awdl_result.has_value()) {
        restarted_mediums.push_back(AWDL);
      } else {
        LOG(WARNING) << "UpdateDiscoveryOptionsImpl: unable to restart "
                        "awdl scanning";
      }
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, AWDL, update_index,
                  awdl_result.has_error()
                      ? awdl_result.error().operation_result_code().value()
                      : OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    }
  }
  // wifi lan
  if (new_mediums.wifi_lan && !discovery_options.low_power) {
    should_start_discovery = true;
    if (!needs_restart && old_mediums.wifi_lan) {
      restarted_mediums.push_back(WIFI_LAN);
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, WIFI_LAN, update_index,
                  OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    } else {
      ErrorOr<Medium> wifi_lan_result =
          StartWifiLanDiscovery(client, std::string(service_id));
      if (wifi_lan_result.has_value()) {
        restarted_mediums.push_back(WIFI_LAN);
      } else {
        LOG(WARNING) << "UpdateDiscoveryOptionsImpl: unable to restart "
                        "wifi lan scanning";
      }
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, WIFI_LAN, update_index,
                  wifi_lan_result.has_error()
                      ? wifi_lan_result.error().operation_result_code().value()
                      : OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    }
  }
  if (restarted_mediums.empty() && should_start_discovery) {
    // All radios failed to start.
    return StartOperationResult{.status = {Status::kError},
                                .mediums = restarted_mediums,
                                .operation_result_with_mediums =
                                    std::move(operation_result_with_mediums)};
  }
  return StartOperationResult{.status = {Status::kSuccess},
                              .mediums = restarted_mediums,
                              .operation_result_with_mediums =
                                  std::move(operation_result_with_mediums)};
}

void P2pClusterPcpHandler::BluetoothConnectionAcceptedHandler(
    ClientProxy* client, absl::string_view local_endpoint_info,
    NearbyDevice::Type device_type, const std::string& service_id,
    BluetoothSocket socket) {
  if (!socket.IsValid()) {
    LOG(WARNING) << "Invalid socket in accept callback("
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
                                 BLUETOOTH, device_type);
          });
}

ErrorOr<Medium> P2pClusterPcpHandler::StartBluetoothAdvertising(
    ClientProxy* client, const std::string& service_id,
    const ByteArray& service_id_hash, const std::string& local_endpoint_id,
    const ByteArray& local_endpoint_info, WebRtcState web_rtc_state) {
  // Start listening for connections before advertising in case a connection
  // request comes in very quickly.
  LOG(INFO) << "P2pClusterPcpHandler::StartBluetoothAdvertising: service="
            << service_id << ": start";
  if (!bluetooth_medium_.IsAcceptingConnections(service_id)) {
    ErrorOr<bool> error = true;
    // TODO(b/380411884): Remove this check since we shouldn't enable radio by
    // NC.
    if (!bluetooth_radio_.Enable()) {
      error = {Error(OperationResultCode::DEVICE_STATE_RADIO_ENABLING_FAILURE)};
    } else {
      ErrorOr<bool> accept_result = bluetooth_medium_.StartAcceptingConnections(
          service_id,
          absl::bind_front(
              &P2pClusterPcpHandler::BluetoothConnectionAcceptedHandler, this,
              client, local_endpoint_info.AsStringView(),
              NearbyDevice::Type::kConnectionsDevice));
      if (accept_result.has_error()) {
        error = {Error(accept_result.error().operation_result_code().value())};
      }
    }
    if (error.has_error()) {
      LOG(WARNING) << "In StartBluetoothAdvertising("
                   << absl::BytesToHexString(local_endpoint_info.data())
                   << "), client=" << client->GetClientId()
                   << " failed to start listening for incoming Bluetooth "
                      "connections to service_id="
                   << service_id;
      return {Error(error.error().operation_result_code().value())};
    }
    LOG(INFO) << "In StartBluetoothAdvertising("
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
    LOG(WARNING) << "In StartBluetoothAdvertising("
                 << absl::BytesToHexString(local_endpoint_info.data())
                 << "), client=" << client->GetClientId()
                 << " failed to generate BluetoothDeviceName {version="
                 << static_cast<int>(kBluetoothDeviceNameVersion)
                 << ", pcp=" << PcpToStrategy(GetPcp()).GetName()
                 << ", endpoint_id=" << local_endpoint_id
                 << ", service_id_hash="
                 << absl::BytesToHexString(service_id_hash.data())
                 << ", endpoint_info="
                 << absl::BytesToHexString(local_endpoint_info.data()) << "}.";
    bluetooth_medium_.StopAcceptingConnections(service_id);
    return {Error(
        OperationResultCode::NEARBY_BLUETOOTH_ADVERTISE_TO_BYTES_FAILURE)};
  }
  LOG(INFO) << "In StartBluetoothAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " generated BluetoothDeviceName " << device_name
            << " with service_id=" << service_id;

  // Become Bluetooth discoverable.
  ErrorOr<bool> bluetooth_result =
      bluetooth_medium_.TurnOnDiscoverability(device_name);
  if (bluetooth_result.has_error()) {
    LOG(INFO)
        << "In StartBluetoothAdvertising("
        << absl::BytesToHexString(local_endpoint_info.data())
        << "), client=" << client->GetClientId()
        << " couldn't start Bluetooth advertising with BluetoothDeviceName "
        << device_name;
    bluetooth_medium_.StopAcceptingConnections(service_id);
    return {Error(bluetooth_result.error().operation_result_code().value())};
  }
  LOG(INFO) << "In StartBluetoothAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " started Bluetooth advertising with BluetoothDeviceName "
            << device_name;
  return {BLUETOOTH};
}

ErrorOr<Medium> P2pClusterPcpHandler::StartBluetoothDiscovery(
    ClientProxy* client, const std::string& service_id) {
  // TODO(b/380411884): Remove this check since we shouldn't enable radio by NC.
  if (!bluetooth_radio_.Enable()) {
    LOG(INFO) << "In StartBluetoothDiscovery(), client="
              << client->GetClientId()
              << " couldn't start scanning on Bluetooth for service_id="
              << service_id;
    return {Error(OperationResultCode::DEVICE_STATE_RADIO_ENABLING_FAILURE)};
  }

  ErrorOr<bool> result = bluetooth_medium_.StartDiscovery(
      service_id,
      {
          .device_discovered_cb = absl::bind_front(
              &P2pClusterPcpHandler::BluetoothDeviceDiscoveredHandler, this,
              client, service_id),
          .device_name_changed_cb = absl::bind_front(
              &P2pClusterPcpHandler::BluetoothNameChangedHandler, this, client,
              service_id),
          .device_lost_cb = absl::bind_front(
              &P2pClusterPcpHandler::BluetoothDeviceLostHandler, this, client,
              service_id),
      });
  if (result.has_error()) {
    LOG(INFO) << "In StartBluetoothDiscovery(), client="
              << client->GetClientId()
              << " couldn't start scanning on Bluetooth for service_id="
              << service_id;
    return {Error(result.error().operation_result_code().value())};
  }

  LOG(INFO) << "In StartBluetoothDiscovery(), client=" << client->GetClientId()
            << " started scanning for Bluetooth for service_id=" << service_id;
  return {BLUETOOTH};
}

void P2pClusterPcpHandler::StartBluetoothDiscoveryWithPause(
    ClientProxy* client, const std::string& service_id,
    const DiscoveryOptions& discovery_options,
    std::vector<Medium>& mediums_started_successfully,
    std::vector<ConnectionsLog::OperationResultWithMedium>&
        operation_result_with_mediums,
    int update_index) {
  if (bluetooth_radio_.IsEnabled()) {
    if (ble_v2_medium_.IsExtendedAdvertisementsAvailable() &&
        std::find(mediums_started_successfully.begin(),
                  mediums_started_successfully.end(),
                  BLE) != mediums_started_successfully.end()) {
      if (bluetooth_medium_.IsDiscovering(service_id)) {
        // If we are already discovering, we don't need to start again.
        ErrorOr<Medium> bluetooth_result =
            StartBluetoothDiscovery(client, service_id);
        if (bluetooth_result.has_value()) {
          LOG(INFO) << "P2pClusterPcpHandler::"
                       "StartBluetoothDiscoveryWithPause: BT added";
          mediums_started_successfully.push_back(*bluetooth_result);
          bluetooth_classic_client_id_to_service_id_map_.insert(
              {client->GetClientId(), service_id});
        }
        std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
            operation_result_with_medium =
                GetOperationResultWithMediumByResultCode(
                    client, BLUETOOTH, update_index,
                    bluetooth_result.has_error()
                        ? bluetooth_result.error()
                              .operation_result_code()
                              .value()
                        : OperationResultCode::DETAIL_SUCCESS);
        operation_result_with_mediums.push_back(*operation_result_with_medium);
      } else {
        LOG(INFO) << "Pause bluetooth discovery for service id : "
                  << service_id;
        paused_bluetooth_clients_discoveries_.insert({service_id, client});
      }
    } else {
      // Always start bluetooth discovery if BLE doesn't support extended
      // advertisements.
      ErrorOr<Medium> bluetooth_result =
          StartBluetoothDiscovery(client, service_id);
      if (bluetooth_result.has_value()) {
        LOG(INFO) << "P2pClusterPcpHandler::"
                     "StartBluetoothDiscoveryWithPause: BT added";
        mediums_started_successfully.push_back(*bluetooth_result);
        bluetooth_classic_client_id_to_service_id_map_.insert(
            {client->GetClientId(), service_id});
      }
      std::unique_ptr<ConnectionsLog::OperationResultWithMedium>
          operation_result_with_medium =
              GetOperationResultWithMediumByResultCode(
                  client, BLUETOOTH, update_index,
                  bluetooth_result.has_error()
                      ? bluetooth_result.error().operation_result_code().value()
                      : OperationResultCode::DETAIL_SUCCESS);
      operation_result_with_mediums.push_back(*operation_result_with_medium);
    }
  } else {
    LOG(WARNING) << "Ignore to discover on bluetooth for service id: "
                 << service_id
                 << " because bluetooth is disabled or low power mode.";
  }
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::BluetoothConnectImpl(
    ClientProxy* client, BluetoothEndpoint* endpoint) {
  VLOG(1) << "Client " << client->GetClientId()
          << " is attempting to connect to endpoint(id="
          << endpoint->endpoint_id << ") over Bluetooth Classic.";
  BluetoothDevice& device = endpoint->bluetooth_device;

  ErrorOr<BluetoothSocket> bluetooth_socket_result = bluetooth_medium_.Connect(
      device, endpoint->service_id,
      client->GetCancellationFlag(endpoint->endpoint_id));
  if (bluetooth_socket_result.has_error()) {
    LOG(ERROR)
        << "In BluetoothConnectImpl(), failed to connect to Bluetooth device "
        << device.GetName() << " for endpoint(id=" << endpoint->endpoint_id
        << ").";
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kBluetoothError},
        .operation_result_code =
            bluetooth_socket_result.error().operation_result_code().value()};
  }

  auto channel = std::make_unique<BluetoothEndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id,
      bluetooth_socket_result.value());
  VLOG(1) << "Client" << client->GetClientId()
          << " created Bluetooth endpoint channel to endpoint(id="
          << endpoint->endpoint_id << ").";
  client->SetBluetoothMacAddress(endpoint->endpoint_id, device.GetAddress());
  return BasePcpHandler::ConnectImplResult{
      .medium = BLUETOOTH,
      .status = {Status::kSuccess},
      .operation_result_code = OperationResultCode::DETAIL_SUCCESS,
      .endpoint_channel = std::move(channel)};
}

void P2pClusterPcpHandler::BleConnectionAcceptedHandler(
    ClientProxy* client, absl::string_view local_endpoint_info,
    NearbyDevice::Type device_type, BleSocket socket,
    const std::string& service_id) {
  if (!socket.IsValid()) {
    LOG(WARNING) << "Invalid socket in accept callback("
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
                                 std::move(channel), BLE, device_type);
          });
}

ErrorOr<Medium> P2pClusterPcpHandler::StartBleAdvertising(
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
  LOG(INFO) << "P2pClusterPcpHandler::StartBleAdvertising: service_id="
            << service_id << " : start";
  if (!ble_medium_.IsAcceptingConnections(service_id)) {
    // TODO(b/380411884): Remove this check since we shouldn't enable radio by
    // NC.
    if (!bluetooth_radio_.Enable()) {
      LOG(WARNING)
          << "In StartBleAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start accepting for incoming BLE connections to "
             "service_id="
          << service_id;
      return {Error(OperationResultCode::DEVICE_STATE_RADIO_ENABLING_FAILURE)};
    }
    ErrorOr<bool> accept_result = ble_medium_.StartAcceptingConnections(
        service_id,
        absl::bind_front(&P2pClusterPcpHandler::BleConnectionAcceptedHandler,
                         this, client, local_endpoint_info.AsStringView(),
                         NearbyDevice::Type::kConnectionsDevice));
    if (!accept_result.has_value()) {
      LOG(WARNING)
          << "In StartBleAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start accepting for incoming BLE connections to "
             "service_id="
          << service_id;
      return {Error(accept_result.error().operation_result_code().value())};
    }
    LOG(INFO)
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
      // TODO(b/380411884): Remove this check since we shouldn't enable radio by
      // NC.
      if (!bluetooth_radio_.Enable()) {
        LOG(WARNING)
            << "In BT StartBleAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " failed to start accepting for incoming BLE connections to "
               "service_id="
            << service_id;
        return {
            Error(OperationResultCode::DEVICE_STATE_RADIO_ENABLING_FAILURE)};
      }
      ErrorOr<bool> accept_result = bluetooth_medium_.StartAcceptingConnections(
          service_id,
          absl::bind_front(
              &P2pClusterPcpHandler::BluetoothConnectionAcceptedHandler, this,
              client, local_endpoint_info.AsStringView(),
              NearbyDevice::Type::kConnectionsDevice));
      if (!accept_result.has_value()) {
        LOG(WARNING)
            << "In BT StartBleAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " failed to start accepting for incoming BLE connections to "
               "service_id="
            << service_id;
        ble_medium_.StopAcceptingConnections(service_id);
        return {Error(accept_result.error().operation_result_code().value())};
      }
      LOG(INFO)
          << "In BT StartBleAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " started accepting for incoming BLE connections to service_id="
          << service_id;
    }
  }

  LOG(INFO) << "In StartBleAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " start to generate BleAdvertisement with service_id="
            << service_id << ", local endpoint_id=" << local_endpoint_id;
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
    MacAddress bluetooth_mac_address;
    if (bluetooth_medium_.IsAvailable() &&
        ShouldAdvertiseBluetoothMacOverBle(power_level))
      bluetooth_mac_address = bluetooth_medium_.GetAddress();

    advertisement_bytes = ByteArray(
        BleAdvertisement(kBleAdvertisementVersion, GetPcp(), service_id_hash,
                         local_endpoint_id, local_endpoint_info,
                         bluetooth_mac_address, ByteArray{}, web_rtc_state));
  }
  if (advertisement_bytes.Empty()) {
    LOG(WARNING) << "In StartBleAdvertising("
                 << absl::BytesToHexString(local_endpoint_info.data())
                 << "), client=" << client->GetClientId()
                 << " failed to create an advertisement.";
    ble_medium_.StopAcceptingConnections(service_id);
    return {Error(OperationResultCode::NEARBY_BLE_ADVERTISE_TO_BYTES_FAILURE)};
  }

  LOG(INFO) << "In StartBleAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " generated BleAdvertisement with service_id=" << service_id
            << ", bytes: "
            << absl::BytesToHexString(advertisement_bytes.data());

  ErrorOr<bool> ble_result = ble_medium_.StartAdvertising(
      service_id, advertisement_bytes,
      advertising_options.fast_advertisement_service_uuid);
  if (ble_result.has_error()) {
    LOG(WARNING) << "In StartBleAdvertising("
                 << absl::BytesToHexString(local_endpoint_info.data())
                 << "), client=" << client->GetClientId()
                 << " couldn't start BLE Advertising with BleAdvertisement "
                 << absl::BytesToHexString(advertisement_bytes.data());
    ble_medium_.StopAcceptingConnections(service_id);
    return {Error(ble_result.error().operation_result_code().value())};
  }
  LOG(INFO) << "In startBleAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << ", fast_advertisement: " << fast_advertisement
            << "), client=" << client->GetClientId()
            << " started BLE Advertising with BleAdvertisement "
            << absl::BytesToHexString(advertisement_bytes.data());
  return {BLE};
}

ErrorOr<Medium> P2pClusterPcpHandler::StartBleScanning(
    ClientProxy* client, const std::string& service_id,
    const std::string& fast_advertisement_service_uuid) {
  // TODO(b/380411884): Remove this check since we shouldn't enable radio by NC.
  if (!bluetooth_radio_.Enable()) {
    LOG(INFO) << "In StartBleScanning(), client=" << client->GetClientId()
              << " couldn't start scanning on BLE for service_id="
              << service_id;
    return {Error(OperationResultCode::DEVICE_STATE_RADIO_ENABLING_FAILURE)};
  }
  ErrorOr<bool> result = ble_medium_.StartScanning(
      service_id, fast_advertisement_service_uuid,
      {
          .peripheral_discovered_cb = absl::bind_front(
              &P2pClusterPcpHandler::BlePeripheralDiscoveredHandler, this,
              client),
          .peripheral_lost_cb = absl::bind_front(
              &P2pClusterPcpHandler::BlePeripheralLostHandler, this, client),
      });
  if (!result.has_error()) {
    LOG(INFO) << "In StartBleScanning(), client=" << client->GetClientId()
              << " started scanning for BLE advertisements for service_id="
              << service_id;
    return {BLE};
  } else {
    LOG(INFO) << "In StartBleScanning(), client=" << client->GetClientId()
              << " couldn't start scanning on BLE for service_id="
              << service_id;
    return {Error(result.error().operation_result_code().value())};
  }
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::BleConnectImpl(
    ClientProxy* client, BleEndpoint* endpoint) {
  VLOG(1) << "Client " << client->GetClientId()
          << " is attempting to connect to endpoint(id="
          << endpoint->endpoint_id << ") over BLE.";

  BlePeripheral& peripheral = endpoint->ble_peripheral;

  ErrorOr<BleSocket> ble_socket_result =
      ble_medium_.Connect(peripheral, endpoint->service_id,
                          client->GetCancellationFlag(endpoint->endpoint_id));
  if (ble_socket_result.has_error()) {
    LOG(ERROR) << "In BleConnectImpl(), failed to connect to BLE device "
               << peripheral.GetName()
               << " for endpoint(id=" << endpoint->endpoint_id << ").";
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kBleError},
        .operation_result_code =
            ble_socket_result.error().operation_result_code().value(),
    };
  }

  auto channel = std::make_unique<BleEndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id,
      ble_socket_result.value());

  return BasePcpHandler::ConnectImplResult{
      .medium = BLE,
      .status = {Status::kSuccess},
      .operation_result_code = OperationResultCode::DETAIL_SUCCESS,
      .endpoint_channel = std::move(channel),
  };
}

void P2pClusterPcpHandler::BleV2ConnectionAcceptedHandler(
    ClientProxy* client, absl::string_view local_endpoint_info,
    NearbyDevice::Type device_type, BleV2Socket socket,
    const std::string& service_id) {
  if (!socket.IsValid()) {
    LOG(WARNING) << "Invalid socket in accept callback("
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
                             BLE, device_type);
      });
}

void P2pClusterPcpHandler::BleL2capConnectionAcceptedHandler(
    ClientProxy* client, absl::string_view local_endpoint_info,
    NearbyDevice::Type device_type, BleL2capSocket socket,
    const std::string& service_id) {
  if (!socket.IsValid()) {
    LOG(WARNING) << "Invalid socket in accept L2CAP callback("
                 << absl::BytesToHexString(local_endpoint_info)
                 << "), client=" << client->GetClientId();
    return;
  }
  RunOnPcpHandlerThread(
      "p2p-ble-l2cap-on-incoming-connection",
      [this, client, service_id, device_type,
       socket = std::move(socket)]() RUN_ON_PCP_HANDLER_THREAD() mutable {
        ByteArray remote_peripheral_info = socket.GetRemotePeripheral().GetId();
        auto channel = std::make_unique<BleL2capEndpointChannel>(
            service_id, std::string(remote_peripheral_info), socket);

        OnIncomingConnection(client, remote_peripheral_info, std::move(channel),
                             BLE, device_type);
      });
}

ErrorOr<Medium> P2pClusterPcpHandler::StartBleV2Advertising(
    ClientProxy* client, const std::string& service_id,
    const std::string& local_endpoint_id, const ByteArray& local_endpoint_info,
    const AdvertisingOptions& advertising_options, WebRtcState web_rtc_state) {
  // Start listening for connections before advertising in case a connection
  // request comes in very quickly. BLE allows connecting over BLE itself, as
  // well as advertising the Bluetooth MAC address to allow connecting over
  // Bluetooth Classic.
  LOG(INFO) << "P2pClusterPcpHandler::StartBleV2Advertising: service_id="
            << service_id << " : start";

  ErrorOr<int> ble_l2cap_result = 0;

  if (!ble_v2_medium_.IsAcceptingConnections(service_id)) {
    // TODO(b/380411884): Remove this check since we shouldn't enable radio by
    // NC.
    if (!bluetooth_radio_.Enable()) {
      LOG(WARNING)
          << "In StartBleV2Advertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start accepting for incoming BLE connections to "
             "service_id="
          << service_id;
      return {Error(OperationResultCode::DEVICE_STATE_RADIO_ENABLING_FAILURE)};
    }
    if (NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_connections_feature::
                kEnableBleL2cap)) {
      ble_l2cap_result = ble_v2_medium_.StartAcceptingL2capConnections(
          service_id,
          absl::bind_front(
              &P2pClusterPcpHandler::BleL2capConnectionAcceptedHandler, this,
              client, local_endpoint_info.AsStringView(),
              NearbyDevice::Type::kConnectionsDevice));
    }

    ErrorOr<bool> ble_v2_result = ble_v2_medium_.StartAcceptingConnections(
        service_id,
        absl::bind_front(&P2pClusterPcpHandler::BleV2ConnectionAcceptedHandler,
                         this, client, local_endpoint_info.AsStringView(),
                         NearbyDevice::Type::kConnectionsDevice));
    if (ble_v2_result.has_error() && ble_l2cap_result.has_error()) {
      LOG(WARNING)
          << "In StartBleV2Advertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start accepting for incoming BLE connections to "
             "service_id="
          << service_id;
      return {Error(ble_v2_result.error().operation_result_code().value())};
    }
    LOG(INFO)
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
      // TODO(b/380411884): Remove this check since we shouldn't enable radio by
      // NC.
      if (!bluetooth_radio_.Enable()) {
        LOG(WARNING)
            << "In BT StartBleV2Advertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " failed to start accepting for incoming BLE connections to "
               "service_id="
            << service_id;
        return {
            Error(OperationResultCode::DEVICE_STATE_RADIO_ENABLING_FAILURE)};
      }
      ErrorOr<bool> accept_result = bluetooth_medium_.StartAcceptingConnections(
          service_id,
          absl::bind_front(
              &P2pClusterPcpHandler::BluetoothConnectionAcceptedHandler, this,
              client, local_endpoint_info.AsStringView(),
              NearbyDevice::Type::kConnectionsDevice));
      if (accept_result.has_error()) {
        LOG(WARNING)
            << "In BT StartBleV2Advertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " failed to start accepting for incoming BLE connections to "
               "service_id="
            << service_id;
        ble_v2_medium_.StopAcceptingConnections(service_id);
        return {Error(accept_result.error().operation_result_code().value())};
      }
      LOG(INFO)
          << "In BT StartBleV2Advertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " started accepting for incoming BLE connections to service_id="
          << service_id;
    }
  }

  LOG(INFO) << "In StartBleV2Advertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " start to generate BleAdvertisement with service_id="
            << service_id << ", local endpoint_id=" << local_endpoint_id;
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
    MacAddress bluetooth_mac_address;
    if (bluetooth_medium_.IsAvailable() &&
        ShouldAdvertiseBluetoothMacOverBle(power_level))
      bluetooth_mac_address = bluetooth_medium_.GetAddress();

    advertisement_bytes = ByteArray(BleAdvertisement(
        kBleAdvertisementVersion, GetPcp(), service_id_hash, local_endpoint_id,
        local_endpoint_info, bluetooth_mac_address,
        /*uwb_address=*/ByteArray{}, web_rtc_state));
  }

  ByteArray dct_advertisement_bytes;
  if (client->IsDctEnabled()) {
    // Try to read device name from local_endpoint_info.
    std::optional<std::string> device_name =
        advertisements::ReadDeviceName(local_endpoint_info);
    uint16_t psm = ble_l2cap_result.value();
    if (device_name.has_value()) {
      std::optional<advertisements::ble::DctAdvertisement> dct_advertisement =
          advertisements::ble::DctAdvertisement::Create(
              service_id, *device_name, psm, client->GetDctDedup());
      if (dct_advertisement.has_value()) {
        dct_advertisement_bytes = ByteArray(dct_advertisement->ToData());
      }
    }
  }
  if (advertisement_bytes.Empty() && dct_advertisement_bytes.Empty()) {
    LOG(WARNING) << "In StartBleV2Advertising("
                 << absl::BytesToHexString(local_endpoint_info.data())
                 << "), client=" << client->GetClientId()
                 << " failed to create an advertisement.";
    ble_v2_medium_.StopAcceptingConnections(service_id);
    return {Error(OperationResultCode::NEARBY_BLE_ADVERTISE_TO_BYTES_FAILURE)};
  }

  LOG(INFO) << "In StartBleV2Advertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " generated BleAdvertisement with service_id=" << service_id;

  ErrorOr<bool> ble_v2_result = false;
  if (dct_advertisement_bytes.Empty()) {
    ble_v2_result = ble_v2_medium_.StartAdvertising(
        service_id, power_level,
        advertising_options.fast_advertisement_service_uuid.empty()
            ? BleV2::AdvertisingType::kRegular
            : BleV2::AdvertisingType::kFast,
        advertisement_bytes);
  } else {
    ble_v2_result = ble_v2_medium_.StartAdvertising(
        service_id, power_level, BleV2::AdvertisingType::kDct,
        dct_advertisement_bytes);
  }
  if (ble_v2_result.has_error()) {
    LOG(WARNING) << "In StartBleV2Advertising("
                 << absl::BytesToHexString(local_endpoint_info.data())
                 << "), client=" << client->GetClientId()
                 << " couldn't start BLE Advertising with BleAdvertisement "
                 << absl::BytesToHexString(advertisement_bytes.data());
    ble_v2_medium_.StopAcceptingConnections(service_id);
    return {Error(ble_v2_result.error().operation_result_code().value())};
  }
  LOG(INFO) << "In StartBleV2Advertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " started BLE Advertising with BleAdvertisement "
            << absl::BytesToHexString(advertisement_bytes.data());
  return {BLE};
}

ErrorOr<Medium> P2pClusterPcpHandler::StartBleV2Scanning(
    ClientProxy* client, const std::string& service_id,
    const DiscoveryOptions& discovery_options) {
  PowerLevel power_level = discovery_options.low_power ? PowerLevel::kLowPower
                                                       : PowerLevel::kHighPower;
  // TODO(b/380411884): Remove this check since we shouldn't enable radio by NC.
  if (!bluetooth_radio_.Enable()) {
    LOG(INFO) << "In StartBleV2Scanning(), client=" << client->GetClientId()
              << " couldn't start scanning on BLE for service_id="
              << service_id;
    return {Error(OperationResultCode::DEVICE_STATE_RADIO_ENABLING_FAILURE)};
  }
  if (discovery_options.ble_options.alternate_uuid.has_value()) {
    ble_v2_medium_.AddAlternateUuidForService(
        *discovery_options.ble_options.alternate_uuid, service_id);
  }
  ErrorOr<bool> ble_v2_result = ble_v2_medium_.StartScanning(
      service_id, GetPcp(), power_level, client->IsDctEnabled(),
      {
          .peripheral_discovered_cb = absl::bind_front(
              &P2pClusterPcpHandler::BleV2PeripheralDiscoveredHandler, this,
              client),
          .peripheral_lost_cb = absl::bind_front(
              &P2pClusterPcpHandler::BleV2PeripheralLostHandler, this, client),
          .instant_lost_cb = absl::bind_front(
              &P2pClusterPcpHandler::BleV2InstantLostHandler, this, client),
          .legacy_device_discovered_cb = absl::bind_front(
              &P2pClusterPcpHandler::BleV2LegacyDeviceDiscoveredHandler, this),
      });
  if (!ble_v2_result.has_error()) {
    LOG(INFO) << "In StartBleV2Scanning(), client=" << client->GetClientId()
              << " started scanning for BLE advertisements for service_id="
              << service_id;
    return {BLE};
  }
  LOG(INFO) << "In StartBleV2Scanning(), client=" << client->GetClientId()
            << " couldn't start scanning on BLE for service_id=" << service_id;

  return {Error(ble_v2_result.error().operation_result_code().value())};
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::BleV2ConnectImpl(
    ClientProxy* client, BleV2Endpoint* endpoint) {
  BleV2Peripheral& peripheral = endpoint->ble_peripheral;

  VLOG(1) << "Client " << client->GetClientId()
          << " is attempting to connect to (" << peripheral.ToReadableString()
          << ") over BLE.";

  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::kEnableBleL2cap) &&
      peripheral.GetPsm() !=
          mediums::BleAdvertisementHeader::kDefaultPsmValue) {
    ErrorOr<BleL2capSocket> ble_l2cap_socket_result =
        ble_v2_medium_.ConnectOverL2cap(
            endpoint->service_id, peripheral,
            client->GetCancellationFlag(endpoint->endpoint_id));
    if (!ble_l2cap_socket_result.has_error()) {
      LOG(INFO) << "In BleV2ConnectImpl(), connected to Ble L2CAP device "
                << absl::BytesToHexString(peripheral.GetId().data())
                << " for endpoint(id=" << endpoint->endpoint_id << ").";
      auto channel = std::make_unique<BleL2capEndpointChannel>(
          endpoint->service_id, /*channel_name=*/endpoint->endpoint_id,
          ble_l2cap_socket_result.value());
      return BasePcpHandler::ConnectImplResult{
          .medium = BLE,
          .status = {Status::kSuccess},
          .operation_result_code = OperationResultCode::DETAIL_SUCCESS,
          .endpoint_channel = std::move(channel),
      };
    } else {
      LOG(WARNING) << "In BleV2ConnectImpl(), failed to connect to Ble L2CAP "
                      "device "
                   << absl::BytesToHexString(peripheral.GetId().data())
                   << " for endpoint(id=" << endpoint->endpoint_id << ").";
    }
  }

  ErrorOr<BleV2Socket> ble_socket_result = ble_v2_medium_.Connect(
      endpoint->service_id, peripheral,
      client->GetCancellationFlag(endpoint->endpoint_id));
  if (ble_socket_result.has_error()) {
    LOG(ERROR) << "In BleV2ConnectImpl(), failed to connect to BLE device "
               << absl::BytesToHexString(peripheral.GetId().data())
               << " for endpoint(id=" << endpoint->endpoint_id << ").";
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kBleError},
        .operation_result_code =
            ble_socket_result.error().operation_result_code().value(),
    };
  }

  auto channel = std::make_unique<BleV2EndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id,
      ble_socket_result.value());

  return BasePcpHandler::ConnectImplResult{
      .medium = BLE,
      .status = {Status::kSuccess},
      .operation_result_code = OperationResultCode::DETAIL_SUCCESS,
      .endpoint_channel = std::move(channel),
  };
}

void P2pClusterPcpHandler::AwdlConnectionAcceptedHandler(
    ClientProxy* client, absl::string_view local_endpoint_id,
    absl::string_view local_endpoint_info, NearbyDevice::Type device_type,
    const std::string& service_id, AwdlSocket socket) {
  if (!socket.IsValid()) {
    LOG(WARNING) << "Invalid socket in accept callback("
                 << absl::BytesToHexString(local_endpoint_info)
                 << "), client=" << client->GetClientId();
    return;
  }
  RunOnPcpHandlerThread(
      "p2p-awdl-on-incoming-connection",
      [this, client, local_endpoint_id, service_id, device_type,
       socket = std::move(socket)]() RUN_ON_PCP_HANDLER_THREAD() mutable {
        std::string remote_service_name = std::string(local_endpoint_id);
        auto channel = std::make_unique<AwdlEndpointChannel>(
            service_id, /*channel_name=*/remote_service_name, socket);
        ByteArray remote_service_name_byte{remote_service_name};

        OnIncomingConnection(client, remote_service_name_byte,
                             std::move(channel), AWDL, device_type);
      });
}

void P2pClusterPcpHandler::WifiLanConnectionAcceptedHandler(
    ClientProxy* client, absl::string_view local_endpoint_id,
    absl::string_view local_endpoint_info, NearbyDevice::Type device_type,
    const std::string& service_id, WifiLanSocket socket) {
  if (!socket.IsValid()) {
    LOG(WARNING) << "Invalid socket in accept callback("
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
                             std::move(channel), WIFI_LAN, device_type);
      });
}

ErrorOr<Medium> P2pClusterPcpHandler::StartAwdlAdvertising(
    ClientProxy* client, const std::string& service_id,
    const std::string& local_endpoint_id, const ByteArray& local_endpoint_info,
    WebRtcState web_rtc_state) {
  //  Start listening for connections before advertising in case a connection
  //  request comes in very quickly.
  LOG(INFO) << "P2pClusterPcpHandler::StartAwdlAdvertising: service="
            << service_id << ": start";
  if (!awdl_medium_.IsAcceptingConnections(service_id)) {
    ErrorOr<bool> awdl_result = awdl_medium_.StartAcceptingConnections(
        service_id,
        absl::bind_front(&P2pClusterPcpHandler::AwdlConnectionAcceptedHandler,
                         this, client, local_endpoint_id,
                         local_endpoint_info.AsStringView(),
                         NearbyDevice::Type::kConnectionsDevice));
    if (awdl_result.has_error()) {
      LOG(WARNING)
          << "In StartAwdlAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start listening for incoming Awdl connections "
             "to service_id="
          << service_id;
      return {Error(awdl_result.error().operation_result_code().value())};
    }
    LOG(INFO) << "In StartAwdlAdvertising("
              << absl::BytesToHexString(local_endpoint_info.data())
              << "), client=" << client->GetClientId()
              << " started listening for incoming Awdl connections "
                 "to service_id = "
              << service_id;
  }

  // Generate a WifiLanServiceInfo with which to become AWDL discoverable.
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
    LOG(WARNING) << "In StartAwdlAdvertising("
                 << absl::BytesToHexString(local_endpoint_info.data())
                 << "), client=" << client->GetClientId()
                 << " failed to generate WifiLanServiceInfo {version="
                 << static_cast<int>(kWifiLanServiceInfoVersion)
                 << ", pcp=" << PcpToStrategy(GetPcp()).GetName()
                 << ", endpoint_id=" << local_endpoint_id
                 << ", service_id_hash="
                 << absl::BytesToHexString(service_id_hash.data())
                 << ", endpoint_info="
                 << absl::BytesToHexString(local_endpoint_info.data()) << "}.";
    awdl_medium_.StopAcceptingConnections(service_id);
    return {Error(OperationResultCode::NEARBY_AWDL_ADVERTISE_TO_BYTES_FAILURE)};
  }
  LOG(INFO) << "In StartAwdlAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " generated WifiLanServiceInfo "
            << nsd_service_info.GetServiceName()
            << " with service_id=" << service_id;

  ErrorOr<bool> awdl_result =
      awdl_medium_.StartAdvertising(service_id, nsd_service_info);
  if (awdl_result.has_error()) {
    LOG(INFO) << "In StartAwdlAdvertising("
              << absl::BytesToHexString(local_endpoint_info.data())
              << "), client=" << client->GetClientId()
              << " couldn't advertise with WifiLanServiceInfo "
              << nsd_service_info.GetServiceName();
    awdl_medium_.StopAcceptingConnections(service_id);
    return {Error(awdl_result.error().operation_result_code().value())};
  }
  LOG(INFO) << "In StartAwdlAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " advertised with WifiLanServiceInfo "
            << nsd_service_info.GetServiceName();
  return {AWDL};
}

ErrorOr<Medium> P2pClusterPcpHandler::StartWifiLanAdvertising(
    ClientProxy* client, const std::string& service_id,
    const std::string& local_endpoint_id, const ByteArray& local_endpoint_info,
    WebRtcState web_rtc_state) {
  // Start listening for connections before advertising in case a connection
  // request comes in very quickly.
  LOG(INFO) << "P2pClusterPcpHandler::StartWifiLanAdvertising: service="
            << service_id << ": start";
  if (!wifi_lan_medium_.IsAcceptingConnections(service_id)) {
    ErrorOr<bool> wifi_lan_result = wifi_lan_medium_.StartAcceptingConnections(
        service_id,
        absl::bind_front(
            &P2pClusterPcpHandler::WifiLanConnectionAcceptedHandler, this,
            client, local_endpoint_id, local_endpoint_info.AsStringView(),
            NearbyDevice::Type::kConnectionsDevice));
    if (wifi_lan_result.has_error()) {
      LOG(WARNING)
          << "In StartWifiLanAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " failed to start listening for incoming WifiLan connections "
             "to service_id="
          << service_id;
      return {Error(wifi_lan_result.error().operation_result_code().value())};
    }
    VLOG(1) << "In StartWifiLanAdvertising("
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
    LOG(WARNING) << "In StartWifiLanAdvertising("
                 << absl::BytesToHexString(local_endpoint_info.data())
                 << "), client=" << client->GetClientId()
                 << " failed to generate WifiLanServiceInfo {version="
                 << static_cast<int>(kWifiLanServiceInfoVersion)
                 << ", pcp=" << PcpToStrategy(GetPcp()).GetName()
                 << ", endpoint_id=" << local_endpoint_id
                 << ", service_id_hash="
                 << absl::BytesToHexString(service_id_hash.data())
                 << ", endpoint_info="
                 << absl::BytesToHexString(local_endpoint_info.data()) << "}.";
    wifi_lan_medium_.StopAcceptingConnections(service_id);
    return {
        Error(OperationResultCode::NEARBY_WIFI_LAN_ADVERTISE_TO_BYTES_FAILURE)};
  }
  LOG(INFO) << "In StartWifiLanAdvertising("
            << absl::BytesToHexString(local_endpoint_info.data())
            << "), client=" << client->GetClientId()
            << " generated WifiLanServiceInfo "
            << nsd_service_info.GetServiceName()
            << " with service_id=" << service_id;

  ErrorOr<bool> wifi_lan_result =
      wifi_lan_medium_.StartAdvertising(service_id, nsd_service_info);
  if (wifi_lan_result.has_error()) {
    LOG(INFO) << "In StartWifiLanAdvertising("
              << absl::BytesToHexString(local_endpoint_info.data())
              << "), client=" << client->GetClientId()
              << " couldn't advertise with WifiLanServiceInfo "
              << nsd_service_info.GetServiceName();
    wifi_lan_medium_.StopAcceptingConnections(service_id);
    return {Error(wifi_lan_result.error().operation_result_code().value())};
  }
  VLOG(1) << "In StartWifiLanAdvertising("
          << absl::BytesToHexString(local_endpoint_info.data())
          << "), client=" << client->GetClientId()
          << " advertised with WifiLanServiceInfo "
          << nsd_service_info.GetServiceName();
  return {WIFI_LAN};
}

ErrorOr<Medium> P2pClusterPcpHandler::StartAwdlDiscovery(
    ClientProxy* client, const std::string& service_id) {
  ErrorOr<bool> result = awdl_medium_.StartDiscovery(
      service_id,
      {
          .service_discovered_cb = absl::bind_front(
              &P2pClusterPcpHandler::AwdlServiceDiscoveredHandler, this,
              client),
          .service_lost_cb = absl::bind_front(
              &P2pClusterPcpHandler::AwdlServiceLostHandler, this, client),
      });
  if (!result.has_error()) {
    LOG(INFO) << "In StartAwdlDiscovery(), client=" << client->GetClientId()
              << " started scanning for Wifi devices for service_id="
              << service_id;
    return {AWDL};
  } else {
    LOG(INFO) << "In StartAwdlDiscovery(), client=" << client->GetClientId()
              << " couldn't start scanning on Wifi for service_id="
              << service_id;
    return {Error(result.error().operation_result_code().value())};
  }
}

ErrorOr<Medium> P2pClusterPcpHandler::StartWifiLanDiscovery(
    ClientProxy* client, const std::string& service_id) {
  ErrorOr<bool> result = wifi_lan_medium_.StartDiscovery(
      service_id,
      {
          .service_discovered_cb = absl::bind_front(
              &P2pClusterPcpHandler::WifiLanServiceDiscoveredHandler, this,
              client),
          .service_lost_cb = absl::bind_front(
              &P2pClusterPcpHandler::WifiLanServiceLostHandler, this, client),
      });
  if (!result.has_error()) {
    LOG(INFO) << "In StartWifiLanDiscovery(), client=" << client->GetClientId()
              << " started scanning for Wifi devices for service_id="
              << service_id;
    return {WIFI_LAN};
  } else {
    LOG(INFO) << "In StartWifiLanDiscovery(), client=" << client->GetClientId()
              << " couldn't start scanning on Wifi for service_id="
              << service_id;
    return {Error(result.error().operation_result_code().value())};
  }
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::AwdlConnectImpl(
    ClientProxy* client, AwdlEndpoint* endpoint) {
  LOG(INFO) << "Client " << client->GetClientId()
            << " is attempting to connect to endpoint(id="
            << endpoint->endpoint_id << ") over Awdl.";
  ErrorOr<AwdlSocket> socket_result =
      awdl_medium_.Connect(endpoint->service_id, endpoint->service_info,
                           client->GetCancellationFlag(endpoint->endpoint_id));
  if (socket_result.has_error()) {
    LOG(ERROR) << "In AwdlConnectImpl(), failed to connect to service "
               << endpoint->service_info.GetServiceName()
               << " for endpoint(id=" << endpoint->endpoint_id << ").";
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kWifiLanError},
        .operation_result_code =
            socket_result.error().operation_result_code().value(),
    };
  }
  LOG(INFO) << "In AwdlConnectImpl(), connect to service "
            << " socket=" << &socket_result.value().GetImpl()
            << " for endpoint(id=" << endpoint->endpoint_id << ").";

  auto channel = std::make_unique<AwdlEndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id,
      socket_result.value());
  LOG(INFO) << "Client " << client->GetClientId()
            << " created Awdl endpoint channel to endpoint(id="
            << endpoint->endpoint_id << ").";
  return BasePcpHandler::ConnectImplResult{
      .medium = AWDL,
      .status = {Status::kSuccess},
      .operation_result_code = OperationResultCode::DETAIL_SUCCESS,
      .endpoint_channel = std::move(channel),
  };
}

BasePcpHandler::ConnectImplResult P2pClusterPcpHandler::WifiLanConnectImpl(
    ClientProxy* client, WifiLanEndpoint* endpoint) {
  LOG(INFO) << "Client " << client->GetClientId()
            << " is attempting to connect to endpoint(id="
            << endpoint->endpoint_id << ") over WifiLan.";
  ErrorOr<WifiLanSocket> socket_result = wifi_lan_medium_.Connect(
      endpoint->service_id, endpoint->service_info,
      client->GetCancellationFlag(endpoint->endpoint_id));
  if (socket_result.has_error()) {
    LOG(ERROR) << "In WifiLanConnectImpl(), failed to connect to service "
               << endpoint->service_info.GetServiceName()
               << " for endpoint(id=" << endpoint->endpoint_id << ").";
    return BasePcpHandler::ConnectImplResult{
        .status = {Status::kWifiLanError},
        .operation_result_code =
            socket_result.error().operation_result_code().value(),
    };
  }
  LOG(INFO) << "In WifiLanConnectImpl(), connect to service "
            << " socket=" << &socket_result.value().GetImpl()
            << " for endpoint(id=" << endpoint->endpoint_id << ").";

  auto channel = std::make_unique<WifiLanEndpointChannel>(
      endpoint->service_id, /*channel_name=*/endpoint->endpoint_id,
      socket_result.value());
  LOG(INFO) << "Client " << client->GetClientId()
            << " created WifiLan endpoint channel to endpoint(id="
            << endpoint->endpoint_id << ").";
  return BasePcpHandler::ConnectImplResult{
      .medium = WIFI_LAN,
      .status = {Status::kSuccess},
      .operation_result_code = OperationResultCode::DETAIL_SUCCESS,
      .endpoint_channel = std::move(channel),
  };
}

}  // namespace connections
}  // namespace nearby
