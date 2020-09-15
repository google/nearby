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

#include "platform/api/hash_utils.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
const BluetoothDeviceName::Version::Value
    P2PClusterPCPHandler<Platform>::kBluetoothDeviceNameVersion =
        BluetoothDeviceName::Version::V1;

template <typename Platform>
const BLEAdvertisement::Version::Value
    P2PClusterPCPHandler<Platform>::kBleAdvertisementVersion =
        BLEAdvertisement::Version::V1;

template <typename Platform>
const WifiLanServiceInfo::Version
    P2PClusterPCPHandler<Platform>::kWifiLanServiceInfoVersion =
        WifiLanServiceInfo::Version::kV1;

template <typename Platform>
ConstPtr<ByteArray> P2PClusterPCPHandler<Platform>::generateHash(
    const string& source, size_t size) {
  // Initiazing a new HashUtils each time instead of making it a class member
  // because FoundBluetoothAdvertisementProcessor uses generateHash in its
  // constructor so this method has to be static. We *could* make a static
  // ScopedPtr for HashUtils, but that can get into dangerous territory in terms
  // of time of destruction of that object, so we'll avoid it for now, and stick
  // with this.
  ScopedPtr<Ptr<HashUtils>> hash_utils(Platform::createHashUtils());

  ScopedPtr<ConstPtr<ByteArray>> scoped_hash(hash_utils->sha256(source));
  return MakeConstPtr(new ByteArray(scoped_hash->getData(), size));
}

template <typename Platform>
P2PClusterPCPHandler<Platform>::P2PClusterPCPHandler(
    Ptr<MediumManager<Platform>> medium_manager,
    Ptr<EndpointManager<Platform>> endpoint_manager,
    Ptr<EndpointChannelManager> endpoint_channel_manager,
    Ptr<BandwidthUpgradeManager> bandwidth_upgrade_manager)
    : BasePCPHandler<Platform>(endpoint_manager, endpoint_channel_manager,
                               bandwidth_upgrade_manager),
      medium_manager_(medium_manager) {}

template <typename Platform>
P2PClusterPCPHandler<Platform>::~P2PClusterPCPHandler() {}

template <typename Platform>
Strategy P2PClusterPCPHandler<Platform>::getStrategy() {
  return Strategy::kP2PCluster;
}

template <typename Platform>
PCP::Value P2PClusterPCPHandler<Platform>::getPCP() {
  return PCP::P2P_CLUSTER;
}

template <typename Platform>
std::vector<proto::connections::Medium>
P2PClusterPCPHandler<Platform>::getConnectionMediumsByPriority() {
  std::vector<proto::connections::Medium> mediums;
  if (medium_manager_->IsWifiLanAvailable()) {
    mediums.push_back(proto::connections::WIFI_LAN);
  }
  if (medium_manager_->isBluetoothAvailable()) {
    mediums.push_back(proto::connections::BLUETOOTH);
  }
  if (medium_manager_->isBleAvailable()) {
    mediums.push_back(proto::connections::BLE);
  }
  return mediums;
}

template <typename Platform>
proto::connections::Medium
P2PClusterPCPHandler<Platform>::getDefaultUpgradeMedium() {
  return proto::connections::WIFI_LAN;
}

template <typename Platform>
Ptr<typename BasePCPHandler<Platform>::StartOperationResult>
P2PClusterPCPHandler<Platform>::startAdvertisingImpl(
    Ptr<ClientProxy<Platform>> client_proxy, const string& service_id,
    const string& local_endpoint_id, const string& local_endpoint_name,
    const AdvertisingOptions& options) {
  std::vector<proto::connections::Medium> mediums_started_successfully;

  ScopedPtr<ConstPtr<ByteArray>> scoped_wifi_lan_service_id_hash(
      generateHash(service_id, WifiLanServiceInfo::kServiceIdHashLength));
  proto::connections::Medium wifi_lan_medium = StartWifiLanAdvertising(
      client_proxy, service_id, scoped_wifi_lan_service_id_hash.get(),
      local_endpoint_id, local_endpoint_name);
  if (proto::connections::UNKNOWN_MEDIUM != wifi_lan_medium) {
    mediums_started_successfully.push_back(wifi_lan_medium);
  }

  ScopedPtr<ConstPtr<ByteArray>> scoped_bluetooth_service_id_hash(
      generateHash(service_id, BluetoothDeviceName::kServiceIdHashLength));
  proto::connections::Medium bluetooth_medium = startBluetoothAdvertising(
      client_proxy, service_id, scoped_bluetooth_service_id_hash.get(),
      local_endpoint_id, local_endpoint_name);
  if (proto::connections::UNKNOWN_MEDIUM != bluetooth_medium) {
    mediums_started_successfully.push_back(bluetooth_medium);
  }

  ScopedPtr<ConstPtr<ByteArray>> scoped_ble_service_id_hash(
      generateHash(service_id, BLEAdvertisement::kServiceIdHashLength));
  proto::connections::Medium ble_medium = startBleAdvertising(
      client_proxy, service_id, scoped_ble_service_id_hash.get(),
      local_endpoint_id, local_endpoint_name);
  if (proto::connections::UNKNOWN_MEDIUM != ble_medium) {
    mediums_started_successfully.push_back(ble_medium);
  }

  if (mediums_started_successfully.empty()) {
    // TODO(tracyzhou): Add logging.
    return BasePCPHandler<Platform>::StartOperationResult::error(
        Status::BLUETOOTH_ERROR);
  }

  // The rest of the operations for startAdvertising() will continue
  // asynchronously via
  // IncomingBluetoothConnectionProcessor.onIncomingBluetoothConnection(), so
  // leave it to that to signal any errors that may occur.
  return BasePCPHandler<Platform>::StartOperationResult::success(
      mediums_started_successfully);
}

template <typename Platform>
Status::Value P2PClusterPCPHandler<Platform>::stopAdvertisingImpl(
    Ptr<ClientProxy<Platform>> client_proxy) {
  medium_manager_->stopBleAdvertising(client_proxy->getAdvertisingServiceId());
  medium_manager_->turnOffBluetoothDiscoverability();
  medium_manager_->StopWifiLanAdvertising(
      client_proxy->getAdvertisingServiceId());
  medium_manager_->stopListeningForIncomingBleConnections(
      client_proxy->getAdvertisingServiceId());
  medium_manager_->stopListeningForIncomingBluetoothConnections(
      client_proxy->getAdvertisingServiceId());
  medium_manager_->StopListeningForIncomingWifiLanConnections(
      client_proxy->getAdvertisingServiceId());
  return Status::SUCCESS;
}

template <typename Platform>
Ptr<typename BasePCPHandler<Platform>::StartOperationResult>
P2PClusterPCPHandler<Platform>::startDiscoveryImpl(
    Ptr<ClientProxy<Platform>> client_proxy, const string& service_id,
    const DiscoveryOptions& options) {
  std::vector<proto::connections::Medium> mediums_started_successfully;

  proto::connections::Medium wifi_lan_medium =
      StartWifiLanDiscovery(MakePtr(new FoundWifiLanServiceProcessor(
                                self_, client_proxy, service_id)),
                            client_proxy, service_id);
  if (proto::connections::UNKNOWN_MEDIUM != wifi_lan_medium) {
    mediums_started_successfully.push_back(wifi_lan_medium);
  }

  proto::connections::Medium bluetooth_medium =
      startBluetoothDiscovery(MakePtr(new FoundBluetoothAdvertisementProcessor(
                                  self_, client_proxy, service_id)),
                              client_proxy, service_id);
  if (proto::connections::UNKNOWN_MEDIUM != bluetooth_medium) {
    mediums_started_successfully.push_back(bluetooth_medium);
  }

  proto::connections::Medium ble_medium = startBleDiscovery(
      MakePtr(new FoundBleAdvertisementProcessor(self_, client_proxy)),
      client_proxy, service_id);
  if (proto::connections::UNKNOWN_MEDIUM != ble_medium) {
    mediums_started_successfully.push_back(ble_medium);
  }

  if (mediums_started_successfully.empty()) {
    // TODO(tracyzhou): Add logging.
    return BasePCPHandler<Platform>::StartOperationResult::error(
        Status::BLUETOOTH_ERROR);
  }

  return BasePCPHandler<Platform>::StartOperationResult::success(
      mediums_started_successfully);
}

template <typename Platform>
Status::Value P2PClusterPCPHandler<Platform>::stopDiscoveryImpl(
    Ptr<ClientProxy<Platform>> client_proxy) {
  medium_manager_->stopBleScanning(client_proxy->getDiscoveryServiceId());
  medium_manager_->stopScanningForBluetoothDevices();
  return Status::SUCCESS;
}

template <typename Platform>
typename BasePCPHandler<Platform>::ConnectImplResult
P2PClusterPCPHandler<Platform>::connectImpl(
    Ptr<ClientProxy<Platform>> client_proxy,
    Ptr<typename BasePCPHandler<Platform>::DiscoveredEndpoint> endpoint) {
  Ptr<WifiLanEndpoint> wifi_lan_endpoint =
      DowncastPtr<WifiLanEndpoint>(endpoint);
  if (!wifi_lan_endpoint.isNull()) {
    return WifiLanConnectImpl(client_proxy, wifi_lan_endpoint);
  }

  Ptr<BluetoothEndpoint> bluetooth_endpoint =
      DowncastPtr<BluetoothEndpoint>(endpoint);
  if (!bluetooth_endpoint.isNull()) {
    return bluetoothConnectImpl(client_proxy, bluetooth_endpoint);
  }

  Ptr<BLEEndpoint> ble_endpoint = DowncastPtr<BLEEndpoint>(endpoint);
  if (!ble_endpoint.isNull()) {
    return bleConnectImpl(client_proxy, ble_endpoint);
  }

  return typename BasePCPHandler<Platform>::ConnectImplResult(
      proto::connections::Medium::UNKNOWN_MEDIUM, Status::ERROR);
}

/////////////////// START IMPLEMENTATIONS FOR NESTED CLASSES ///////////////////

///////// P2PClusterPCPHandler::IncomingBluetoothConnectionProcessor //////////
template <typename Platform>
P2PClusterPCPHandler<Platform>::IncomingBluetoothConnectionProcessor::
    IncomingBluetoothConnectionProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        const string& local_endpoint_name)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      local_endpoint_name_(local_endpoint_name) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::IncomingBluetoothConnectionProcessor::
    onIncomingBluetoothConnection(Ptr<BluetoothSocket> bluetooth_socket) {
  pcp_handler_->runOnPCPHandlerThread(
      MakePtr(new OnIncomingBluetoothConnectionRunnable(
          pcp_handler_, client_proxy_, bluetooth_socket)));
}

template <typename Platform>
P2PClusterPCPHandler<Platform>::IncomingBluetoothConnectionProcessor::
    OnIncomingBluetoothConnectionRunnable::
        OnIncomingBluetoothConnectionRunnable(
            Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
            Ptr<ClientProxy<Platform>> client_proxy,
            Ptr<BluetoothSocket> bluetooth_socket)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      bluetooth_socket_(bluetooth_socket) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::IncomingBluetoothConnectionProcessor::
    OnIncomingBluetoothConnectionRunnable::run() {
  string remote_device_name = bluetooth_socket_->getRemoteDevice()->getName();
  ScopedPtr<Ptr<EndpointChannel>> scoped_bluetooth_endpoint_channel(
      pcp_handler_->endpoint_channel_manager_
          ->createIncomingBluetoothEndpointChannel(remote_device_name,
                                                   bluetooth_socket_));
  if (!scoped_bluetooth_endpoint_channel.isNull()) {
    // TODO(tracyzhou): Add logging.
  } else {
    Exception::Value exception = bluetooth_socket_->close();
    bluetooth_socket_.destroy();
    if (Exception::NONE != exception) {
      if (Exception::IO == exception) {
        // TODO(tracyzhou): Add logging.
      }
    }
  }
  pcp_handler_->onIncomingConnection(
      client_proxy_, remote_device_name,
      scoped_bluetooth_endpoint_channel.release(),
      proto::connections::Medium::BLUETOOTH);
}

//////////// P2PClusterPCPHandler::IncomingBleConnectionProcessor /////////////
template <typename Platform>
P2PClusterPCPHandler<Platform>::IncomingBleConnectionProcessor::
    IncomingBleConnectionProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        const string& local_endpoint_name)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      local_endpoint_name_(local_endpoint_name) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::IncomingBleConnectionProcessor::
    onIncomingBleConnection(Ptr<BLESocket> ble_socket,
                            const string& service_id) {
  pcp_handler_->runOnPCPHandlerThread(
      MakePtr(new OnIncomingBleConnectionRunnable(pcp_handler_, client_proxy_,
                                                  ble_socket)));
}

template <typename Platform>
P2PClusterPCPHandler<Platform>::IncomingBleConnectionProcessor::
    OnIncomingBleConnectionRunnable::OnIncomingBleConnectionRunnable(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy, Ptr<BLESocket> ble_socket)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      ble_socket_(ble_socket) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::IncomingBleConnectionProcessor::
    OnIncomingBleConnectionRunnable::run() {
  string remote_device_name =
      ble_socket_->getRemotePeripheral()->getBluetoothDevice()->getName();
  ScopedPtr<Ptr<EndpointChannel>> scoped_ble_endpoint_channel(
      pcp_handler_->endpoint_channel_manager_->createIncomingBLEEndpointChannel(
          remote_device_name, ble_socket_));
  if (!scoped_ble_endpoint_channel.isNull()) {
    // TODO(ahlee): Add logging.
  } else {
    Exception::Value exception = ble_socket_->close();
    ble_socket_.destroy();
    if (Exception::NONE != exception) {
      if (Exception::IO == exception) {
        // TODO(ahlee): Add logging.
      }
    }
  }
  pcp_handler_->onIncomingConnection(client_proxy_, remote_device_name,
                                     scoped_ble_endpoint_channel.release(),
                                     proto::connections::Medium::BLE);
}

//////////// P2PClusterPCPHandler::IncomingWifiLanConnectionProcessor /////////
template <typename Platform>
P2PClusterPCPHandler<Platform>::IncomingWifiLanConnectionProcessor::
    IncomingWifiLanConnectionProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        absl::string_view local_endpoint_name)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      local_endpoint_name_(local_endpoint_name) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::IncomingWifiLanConnectionProcessor::
    OnIncomingWifiLanConnection(Ptr<WifiLanSocket> wifi_lan_socket) {
  pcp_handler_->runOnPCPHandlerThread(
      MakePtr(new OnIncomingWifiLanConnectionRunnable(
          pcp_handler_, client_proxy_, wifi_lan_socket)));
}

template <typename Platform>
P2PClusterPCPHandler<Platform>::IncomingWifiLanConnectionProcessor::
    OnIncomingWifiLanConnectionRunnable::OnIncomingWifiLanConnectionRunnable(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        Ptr<WifiLanSocket> wifi_lan_socket)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      wifi_lan_socket_(wifi_lan_socket) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::IncomingWifiLanConnectionProcessor::
    OnIncomingWifiLanConnectionRunnable::run() {
  string remote_service_name =
      wifi_lan_socket_->GetRemoteWifiLanService()->GetName();
  ScopedPtr<Ptr<EndpointChannel>> scoped_wifi_lan_endpoint_channel(
      pcp_handler_->endpoint_channel_manager_
          ->CreateIncomingWifiLanEndpointChannel(remote_service_name,
                                                 wifi_lan_socket_));
  if (!scoped_wifi_lan_endpoint_channel.isNull()) {
    // TODO(b/149806065): Add logging.
  } else {
    Exception::Value exception = wifi_lan_socket_->Close();
    wifi_lan_socket_.destroy();
    if (Exception::NONE != exception) {
      if (Exception::IO == exception) {
        // TODO(b/149806065): Add logging.
      }
    }
  }
  pcp_handler_->onIncomingConnection(client_proxy_, remote_service_name,
                                     scoped_wifi_lan_endpoint_channel.release(),
                                     proto::connections::Medium::WIFI_LAN);
}

///////// P2PClusterPCPHandler::FoundBluetoothAdvertisementProcessor //////////
template <typename Platform>
P2PClusterPCPHandler<Platform>::FoundBluetoothAdvertisementProcessor::
    FoundBluetoothAdvertisementProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy, const string& service_id)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      service_id_(service_id),
      expected_service_id_hash_(generateHash(
          service_id, BluetoothDeviceName::kServiceIdHashLength)) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundBluetoothAdvertisementProcessor::
    onFoundBluetoothDevice(Ptr<BluetoothDevice> bluetooth_device) {
  pcp_handler_->runOnPCPHandlerThread(
      MakePtr(new OnFoundBluetoothDeviceRunnable(pcp_handler_, client_proxy_,
                                                 self_, service_id_,
                                                 bluetooth_device)));
}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundBluetoothAdvertisementProcessor::
    onLostBluetoothDevice(Ptr<BluetoothDevice> bluetooth_device) {
  pcp_handler_->runOnPCPHandlerThread(MakePtr(new OnLostBluetoothDeviceRunnable(
      pcp_handler_, client_proxy_, self_, service_id_,
      bluetooth_device)));
}

template <typename Platform>
bool P2PClusterPCPHandler<Platform>::FoundBluetoothAdvertisementProcessor::
    isRecognizedBluetoothEndpoint(
        const string& found_bluetooth_device_name,
        Ptr<BluetoothDeviceName> bluetooth_device_name) {
  if (bluetooth_device_name.isNull()) {
    // TODO(tracyzhou): Add logging.
    return false;
  }

  if (bluetooth_device_name->getPCP() != pcp_handler_->getPCP()) {
    // TODO(tracyzhou): Add logging.
    return false;
  }

  if (*(bluetooth_device_name->getServiceIdHash()) !=
      *(expected_service_id_hash_.get())) {
    // TODO(tracyzhou): Add logging.
    return false;
  }

  return true;
}

template <typename Platform>
P2PClusterPCPHandler<Platform>::FoundBluetoothAdvertisementProcessor::
    OnFoundBluetoothDeviceRunnable::OnFoundBluetoothDeviceRunnable(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        Ptr<FoundBluetoothAdvertisementProcessor>
            found_bluetooth_advertisement_processor,
        const string& service_id, Ptr<BluetoothDevice> bluetooth_device)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      found_bluetooth_advertisement_processor_(
          found_bluetooth_advertisement_processor),
      service_id_(service_id),
      bluetooth_device_(bluetooth_device) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundBluetoothAdvertisementProcessor::
    OnFoundBluetoothDeviceRunnable::run() {
  // Make sure we are still discovering before proceeding.
  if (!client_proxy_->isDiscovering()) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  // Parse the Bluetooth device name.
  ScopedPtr<Ptr<BluetoothDeviceName>> bluetooth_device_name(
      BluetoothDeviceName::fromString(bluetooth_device_->getName()));

  // Make sure the Bluetooth device name points to a valid endpoint we're
  // discovering.
  if (!found_bluetooth_advertisement_processor_->isRecognizedBluetoothEndpoint(
          bluetooth_device_->getName(), bluetooth_device_name.get())) {
    return;
  }

  // Report the discovered endpoint to the client.
  // TODO(tracyzhou): Add logging.
  pcp_handler_->onEndpointFound(
      client_proxy_,
      MakePtr(new BluetoothEndpoint(
          bluetooth_device_.release(), bluetooth_device_name->getEndpointId(),
          bluetooth_device_name->getEndpointName(), service_id_)));
}

template <typename Platform>
P2PClusterPCPHandler<Platform>::FoundBluetoothAdvertisementProcessor::
    OnLostBluetoothDeviceRunnable::OnLostBluetoothDeviceRunnable(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        Ptr<FoundBluetoothAdvertisementProcessor>
            found_bluetooth_advertisement_processor,
        const string& service_id, Ptr<BluetoothDevice> bluetooth_device)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      found_bluetooth_advertisement_processor_(
          found_bluetooth_advertisement_processor),
      service_id_(service_id),
      bluetooth_device_(bluetooth_device) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundBluetoothAdvertisementProcessor::
    OnLostBluetoothDeviceRunnable::run() {
  // Make sure we are still discovering before proceeding.
  if (!client_proxy_->isDiscovering()) {
    // TODO(tracyzhou): Add logging.
    return;
  }

  // Parse the Bluetooth device name.
  ScopedPtr<Ptr<BluetoothDeviceName>> bluetooth_device_name(
      BluetoothDeviceName::fromString(bluetooth_device_->getName()));

  // Make sure the Bluetooth device name points to a valid endpoint we're
  // discovering.
  if (!found_bluetooth_advertisement_processor_->isRecognizedBluetoothEndpoint(
          bluetooth_device_->getName(), bluetooth_device_name.get())) {
    return;
  }

  // Report the endpoint as lost to the client.
  // TODO(tracyzhou): Add logging.
  pcp_handler_->onEndpointLost(
      client_proxy_,
      MakePtr(new BluetoothEndpoint(
          bluetooth_device_.release(), bluetooth_device_name->getEndpointId(),
          bluetooth_device_name->getEndpointName(), service_id_)));
}

//////////// P2PClusterPCPHandler::FoundBleAdvertisementProcessor /////////////
template <typename Platform>
P2PClusterPCPHandler<Platform>::FoundBleAdvertisementProcessor::
    FoundBleAdvertisementProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy)
    : pcp_handler_(pcp_handler), client_proxy_(client_proxy) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundBleAdvertisementProcessor::
    onFoundBlePeripheral(Ptr<BLE_PERIPHERAL> ble_peripheral,
                         const string& service_id,
                         ConstPtr<ByteArray> advertisement_bytes) {
  pcp_handler_->runOnPCPHandlerThread(MakePtr(new OnFoundBlePeripheralRunnable(
      pcp_handler_, client_proxy_, self_, service_id, ble_peripheral,
      advertisement_bytes)));
}

template <typename Platform>
P2PClusterPCPHandler<Platform>::FoundBleAdvertisementProcessor::
    OnFoundBlePeripheralRunnable::OnFoundBlePeripheralRunnable(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        Ptr<FoundBleAdvertisementProcessor> found_ble_advertisement_processor,
        const string& service_id, Ptr<BLE_PERIPHERAL> ble_peripheral,
        ConstPtr<ByteArray> advertisement_bytes)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      found_ble_advertisement_processor_(found_ble_advertisement_processor),
      service_id_(service_id),
      ble_peripheral_(ble_peripheral),
      advertisement_bytes_(advertisement_bytes),
      expected_service_id_hash_(
          generateHash(service_id, BLEAdvertisement::kServiceIdHashLength)) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundBleAdvertisementProcessor::
    OnFoundBlePeripheralRunnable::run() {
  // Make sure we are still discovering before proceeding.
  if (!client_proxy_->isDiscovering()) {
    // TODO(ahlee): logger.atWarning().log("Skipping discovery of
    // BLEAdvertisement header %s because we are no longer discovering.",
    // bytesToString(advertisementBytes));
    return;
  }

  ScopedPtr<Ptr<BLEAdvertisement>> scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(advertisement_bytes_.get()));
  if (scoped_ble_advertisement.isNull()) {
    // TODO(ahlee): logger.atVerbose().log("%s doesn't conform to the
    // BLEAdvertisement format, discarding.",
    // bytesToSTring(advertisementBytes));
    return;
  }

  if (scoped_ble_advertisement->getVersion() != BLEAdvertisement::Version::V1) {
    // TODO(ahlee): logging
    return;
  }

  if (scoped_ble_advertisement->getPCP() != pcp_handler_->getPCP()) {
    // TODO(ahlee): Add logging
    return;
  }

  if (*(scoped_ble_advertisement->getServiceIdHash()) !=
      *(expected_service_id_hash_.get())) {
    // TODO(ahlee): Add logging
    return;
  }

  // TODO(ahlee): Add logging.

  // Store all the state we need to be able to re-create a BLEEndpoint in
  // OnLostBlePeripheralRunnable::run(), since that isn't privy to the bytes of
  // the BLE advertisement itself.
  found_ble_advertisement_processor_->found_ble_endpoints_.insert(
      std::make_pair(
          getBlePeripheralId(ble_peripheral_.get()),
          BLEEndpointState(scoped_ble_advertisement->getEndpointId(),
                           scoped_ble_advertisement->getEndpointName())));

  pcp_handler_->onEndpointFound(
      client_proxy_,
      MakePtr(new BLEEndpoint(
          ble_peripheral_.release(), scoped_ble_advertisement->getEndpointId(),
          scoped_ble_advertisement->getEndpointName(), service_id_)));

  // TODO(b/75047971): Add functionality to connect over Bluetooth.
}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundBleAdvertisementProcessor::
    onLostBlePeripheral(Ptr<BLE_PERIPHERAL> ble_peripheral,
                        const string& service_id) {
  pcp_handler_->runOnPCPHandlerThread(MakePtr(new OnLostBlePeripheralRunnable(
      pcp_handler_, client_proxy_, self_, service_id, ble_peripheral)));
}

template <typename Platform>
P2PClusterPCPHandler<Platform>::FoundBleAdvertisementProcessor::
    OnLostBlePeripheralRunnable::OnLostBlePeripheralRunnable(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        Ptr<FoundBleAdvertisementProcessor> found_ble_advertisement_processor,
        const string& service_id, Ptr<BLE_PERIPHERAL> ble_peripheral)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      found_ble_advertisement_processor_(found_ble_advertisement_processor),
      service_id_(service_id),
      ble_peripheral_(ble_peripheral) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundBleAdvertisementProcessor::
    OnLostBlePeripheralRunnable::run() {
  // Make sure we are still discovering before proceeding.
  if (!client_proxy_->isDiscovering()) {
    // TODO(reznor): logger.atWarning().log("Ignoring lost BlePeripheral %s
    // because we are no longer discovering.", blePeripheral);
    return;
  }

  // Remove this BLEPeripheral from
  // found_ble_advertisement_processor_->found_ble_endpoints_, and report the
  // endpoint as lost to the client.
  typename FoundBLEEndpointsMap::iterator it =
      found_ble_advertisement_processor_->found_ble_endpoints_.find(
          getBlePeripheralId(ble_peripheral_.get()));
  if (it != found_ble_advertisement_processor_->found_ble_endpoints_.end()) {
    // TODO(reznor): logger.atDebug().log("Lost BlePeripheral %s (with
    // EndpointId %s and EndpointName %s)", blePeripheral,
    // bleEndpoint.getEndpointId(), bleEndpoint.getEndpointName());

    // Make a copy since it->second will get destroyed once we call erase()
    // below.
    BLEEndpointState ble_endpoint_state(it->second);
    found_ble_advertisement_processor_->found_ble_endpoints_.erase(it);

    pcp_handler_->onEndpointLost(
        client_proxy_,
        MakePtr(new BLEEndpoint(
            ble_peripheral_.release(), ble_endpoint_state.endpoint_id,
            ble_endpoint_state.endpoint_name, service_id_)));
  }
}

////////// P2PClusterPCPHandler::FoundWifiLanServiceProcessor ///////////
template <typename Platform>
P2PClusterPCPHandler<Platform>::FoundWifiLanServiceProcessor::
    FoundWifiLanServiceProcessor(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy, absl::string_view service_id)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      service_id_(service_id),
      expected_service_id_hash_(generateHash(
          string(service_id), WifiLanServiceInfo::kServiceIdHashLength)) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundWifiLanServiceProcessor::
    OnFoundWifiLanService(Ptr<WifiLanService> wifi_lan_service) {
  pcp_handler_->runOnPCPHandlerThread(MakePtr(new OnFoundWifiLanServiceRunnable(
      pcp_handler_, client_proxy_, self_, service_id_, wifi_lan_service)));
}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundWifiLanServiceProcessor::
    OnLostWifiLanService(Ptr<WifiLanService> wifi_lan_service) {
  pcp_handler_->runOnPCPHandlerThread(MakePtr(new OnLostWifiLanServiceRunnable(
      pcp_handler_, client_proxy_, self_, service_id_, wifi_lan_service)));
}

template <typename Platform>
bool P2PClusterPCPHandler<Platform>::FoundWifiLanServiceProcessor::
    IsRecognizedWifiLanEndpoint(Ptr<WifiLanServiceInfo> wifi_lan_service_info) {
  if (wifi_lan_service_info.isNull()) {
    return false;
  }

  if (wifi_lan_service_info->GetPcp() != pcp_handler_->getPCP()) {
    return false;
  }

  if (*(wifi_lan_service_info->GetServiceIdHash()) !=
      *(expected_service_id_hash_.get())) {
    return false;
  }

  return true;
}

template <typename Platform>
P2PClusterPCPHandler<Platform>::FoundWifiLanServiceProcessor::
    OnFoundWifiLanServiceRunnable::OnFoundWifiLanServiceRunnable(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        Ptr<FoundWifiLanServiceProcessor> found_wifi_lan_service_processor,
        absl::string_view service_id, Ptr<WifiLanService> wifi_lan_service)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      found_wifi_lan_service_processor_(found_wifi_lan_service_processor),
      service_id_(service_id),
      wifi_lan_service_(wifi_lan_service),
      expected_service_id_hash_(generateHash(
          string(service_id), WifiLanServiceInfo::kServiceIdHashLength)) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundWifiLanServiceProcessor::
    OnFoundWifiLanServiceRunnable::run() {
  // Make sure we are still discovering before proceeding.
  if (!client_proxy_->isDiscovering()) {
    return;
  }

  // Parse the WifiLan service name.
  ScopedPtr<Ptr<WifiLanServiceInfo>> wifi_lan_service_info(
      WifiLanServiceInfo::FromString(wifi_lan_service_->GetName()));

  // Make sure the WifiLan service name points to a valid endpoint we're
  // discovering.
  if (!found_wifi_lan_service_processor_->IsRecognizedWifiLanEndpoint(
          wifi_lan_service_info.get())) {
    return;
  }

  // Report the discovered endpoint to the client.
  pcp_handler_->onEndpointFound(
      client_proxy_,
      MakePtr(new WifiLanEndpoint(
          wifi_lan_service_.release(),
          wifi_lan_service_info->GetEndpointId(),
          wifi_lan_service_info->GetEndpointName(), service_id_)));
}

template <typename Platform>
P2PClusterPCPHandler<Platform>::FoundWifiLanServiceProcessor::
    OnLostWifiLanServiceRunnable::OnLostWifiLanServiceRunnable(
        Ptr<P2PClusterPCPHandler<Platform>> pcp_handler,
        Ptr<ClientProxy<Platform>> client_proxy,
        Ptr<FoundWifiLanServiceProcessor> found_wifi_lan_service_processor,
        absl::string_view service_id, Ptr<WifiLanService> wifi_lan_service)
    : pcp_handler_(pcp_handler),
      client_proxy_(client_proxy),
      found_wifi_lan_service_processor_(found_wifi_lan_service_processor),
      service_id_(service_id),
      wifi_lan_service_(wifi_lan_service.operator->()) {}

template <typename Platform>
void P2PClusterPCPHandler<Platform>::FoundWifiLanServiceProcessor::
    OnLostWifiLanServiceRunnable::run() {
  // Make sure we are still discovering before proceeding.
  if (!client_proxy_->isDiscovering()) {
    // TODO(b/149806065): Add logging.
    return;
  }

  // Parse the WifiLan service name.
  ScopedPtr<Ptr<WifiLanServiceInfo>> wifi_lan_service_info(
      WifiLanServiceInfo::FromString(wifi_lan_service_->GetName()));

  // Make sure the WifiLan service name points to a valid endpoint we're
  // discovering.
  if (!found_wifi_lan_service_processor_->IsRecognizedWifiLanEndpoint(
          wifi_lan_service_info.get())) {
    return;
  }

  // Report the endpoint as lost to the client.
  // TODO(b/149806065): Add logging.
  pcp_handler_->onEndpointLost(
      client_proxy_,
      MakePtr(new WifiLanEndpoint(
          Ptr<WifiLanService>(wifi_lan_service_.release()),
          wifi_lan_service_info->GetEndpointId(),
          wifi_lan_service_info->GetEndpointName(), service_id_)));
}

//////////////////// END IMPLEMENTATIONS FOR NESTED CLASSES ////////////////////

template <typename Platform>
proto::connections::Medium
P2PClusterPCPHandler<Platform>::startBluetoothAdvertising(
    Ptr<ClientProxy<Platform>> client_proxy, const string& service_id,
    ConstPtr<ByteArray> service_id_hash, const string& local_endpoint_id,
    const string& local_endpoint_name) {
  // Start listening for connections before advertising in case a connection
  // request comes in very quickly.
  if (!medium_manager_->isListeningForIncomingBluetoothConnections(
          service_id)) {
    if (!medium_manager_->startListeningForIncomingBluetoothConnections(
            service_id,
            MakePtr(new IncomingBluetoothConnectionProcessor(
                self_, client_proxy, local_endpoint_name)))) {
      // TODO(tracyzhou): Add logging.
      return proto::connections::UNKNOWN_MEDIUM;
    }

    // TODO(tracyzhou): Add logging.
  }

  // Generate a BluetoothDeviceName with which to become Bluetooth discoverable.
  const string bluetooth_device_name = BluetoothDeviceName::asString(
      kBluetoothDeviceNameVersion, getPCP(), local_endpoint_id, service_id_hash,
      local_endpoint_name);
  if (bluetooth_device_name.empty()) {
    // TODO(tracyzhou): Add logging.
    medium_manager_->stopListeningForIncomingBluetoothConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  } else {
    // TODO(tracyzhou): Add logging.
  }

  // Become Bluetooth discoverable.
  if (!medium_manager_->turnOnBluetoothDiscoverability(bluetooth_device_name)) {
    // TODO(tracyzhou): Add logging.
    medium_manager_->stopListeningForIncomingBluetoothConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  } else {
    // TODO(tracyzhou): Add logging.
  }
  return proto::connections::BLUETOOTH;
}

template <typename Platform>
proto::connections::Medium
P2PClusterPCPHandler<Platform>::startBluetoothDiscovery(
    Ptr<FoundBluetoothAdvertisementProcessor> processor,
    Ptr<ClientProxy<Platform>> client_proxy, const string& service_id) {
  if (!medium_manager_->startScanningForBluetoothDevices(processor)) {
    // TODO(tracyzhou): Add logging.
    return proto::connections::UNKNOWN_MEDIUM;
  } else {
    // TODO(tracyzhou): Add logging.
  }

  return proto::connections::BLUETOOTH;
}

template <typename Platform>
proto::connections::Medium P2PClusterPCPHandler<Platform>::startBleAdvertising(
    Ptr<ClientProxy<Platform>> client_proxy, const string& service_id,
    ConstPtr<ByteArray> service_id_hash, const string& local_endpoint_id,
    const string& local_endpoint_name) {
  // Start listening for connections before advertising in case a connection
  // request comes in very quickly.
  if (!medium_manager_->isListeningForIncomingBleConnections(service_id)) {
    if (!medium_manager_->startListeningForIncomingBleConnections(
            service_id,
            MakePtr(new IncomingBleConnectionProcessor(
                self_, client_proxy, local_endpoint_name)))) {
      // TODO(ahlee): logger.atWarning().log("In startBleAdvertising(%s), client
      // %d failed to start listening for incoming BLE connections to ServiceId
      // %s", local_endpoint_name, clientProxy.getClientId(), service_id);
      return proto::connections::UNKNOWN_MEDIUM;
    }

    // TODO(ahlee): Add logging.
  }

  // TODO(b/75047971): Add functionality to connect over Bluetooth.

  // Create a BLEAdvertisement.
  // TODO(b/75047971): Add a bluetooth_adapter method to get the mac address.
  string bluetooth_mac_address;
  ScopedPtr<ConstPtr<ByteArray>> scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(kBleAdvertisementVersion, getPCP(),
                                service_id_hash, local_endpoint_id,
                                local_endpoint_name, bluetooth_mac_address));
  if (scoped_ble_advertisement_bytes.isNull()) {
    // TODO(ahlee): Add logging
    medium_manager_->stopListeningForIncomingBleConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }

  // TODO(ahlee): Add logging

  if (!medium_manager_->startBleAdvertising(
          service_id, scoped_ble_advertisement_bytes.release())) {
    // TODO(ahlee): Add logging
    medium_manager_->stopListeningForIncomingBleConnections(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }

  // TODO(ahlee): Add logging
  return proto::connections::BLE;
}

template <typename Platform>
proto::connections::Medium P2PClusterPCPHandler<Platform>::startBleDiscovery(
    Ptr<FoundBleAdvertisementProcessor> processor,
    Ptr<ClientProxy<Platform>> client_proxy, const string& service_id) {
  if (!medium_manager_->startBleScanning(service_id, processor)) {
    // TODO(ahlee): logger.atDebug().log("In startBleDiscover(), client %d
    // couldn't start scanning on BLE for service id %s.",
    // client_proxy.getClientId(), service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }

  // TODO(ahlee): logger.atVerbose().log("In startBleDiscovery(), client %d
  // started scanning for BLE advertisements for serviceId %s.",
  // client_proxy.getClietnId(), service_id);

  return proto::connections::BLE;
}

template <typename Platform>
proto::connections::Medium
P2PClusterPCPHandler<Platform>::StartWifiLanAdvertising(
    Ptr<ClientProxy<Platform>> client_proxy, absl::string_view service_id,
    ConstPtr<ByteArray> service_id_hash, absl::string_view local_endpoint_id,
    absl::string_view local_endpoint_name) {
  // Start listening for connections before advertising in case a connection
  // request comes in very quickly.
  if (!medium_manager_->IsListeningForIncomingWifiLanConnections(service_id)) {
    if (!medium_manager_->StartListeningForIncomingWifiLanConnections(
            service_id, MakePtr(new IncomingWifiLanConnectionProcessor(
                            self_, client_proxy, local_endpoint_name)))) {
      // TODO(b/149806065): logger.atWarning().log("In
      // StartWifiLanAdvertising(%s), client %d failed to start listening for
      // incoming WifiLan connections to ServiceId %s", local_endpoint_name,
      // clientProxy.getClientId(), service_id);
      return proto::connections::UNKNOWN_MEDIUM;
    }

    // TODO(b/149806065): Add logging.
  }

  // Generate a WifiLanServiceInfo.
  const string wifi_lan_service_info =
      WifiLanServiceInfo::AsString(kWifiLanServiceInfoVersion,
                                   getPCP(),
                                   local_endpoint_id,
                                   service_id_hash);
  if (wifi_lan_service_info.empty()) {
    // TODO(b/149806065): Add logging.
    return proto::connections::UNKNOWN_MEDIUM;
  } else {
    // TODO(b/149806065): Add logging.
  }

  // TODO(b/149806065): Add logging

  if (!medium_manager_->StartWifiLanAdvertising(
          service_id, wifi_lan_service_info)) {
    // TODO(b/149806065): Add logging
    medium_manager_->StopWifiLanAdvertising(service_id);
    return proto::connections::UNKNOWN_MEDIUM;
  }
  return proto::connections::WIFI_LAN;
}

template <typename Platform>
proto::connections::Medium
P2PClusterPCPHandler<Platform>::StartWifiLanDiscovery(
      Ptr<FoundWifiLanServiceProcessor> processor,
      Ptr<ClientProxy<Platform> > client_proxy, absl::string_view service_id) {
  if (!medium_manager_->StartWifiLanDiscovery(service_id, processor)) {
    // TODO(b/149806065): Add logging.
    return proto::connections::UNKNOWN_MEDIUM;
  }

  return proto::connections::WIFI_LAN;
}

template <typename Platform>
typename BasePCPHandler<Platform>::ConnectImplResult
P2PClusterPCPHandler<Platform>::bluetoothConnectImpl(
    Ptr<ClientProxy<Platform>> client_proxy,
    Ptr<BluetoothEndpoint> bluetooth_endpoint) {
  Ptr<BluetoothDevice> remote_bluetooth_device =
      bluetooth_endpoint->getBluetoothDevice();

  Ptr<BluetoothSocket> bluetooth_socket =
      medium_manager_->connectToBluetoothDevice(
          remote_bluetooth_device, bluetooth_endpoint->getServiceId());
  if (bluetooth_socket.isNull()) {
    return typename BasePCPHandler<Platform>::ConnectImplResult(
        proto::connections::Medium::BLUETOOTH, Status::BLUETOOTH_ERROR);
  }

  ScopedPtr<Ptr<EndpointChannel>> scoped_bluetooth_endpoint_channel(
      this->endpoint_channel_manager_->createOutgoingBluetoothEndpointChannel(
          bluetooth_endpoint->getEndpointId(), bluetooth_socket));

  if (scoped_bluetooth_endpoint_channel.isNull()) {
    bluetooth_socket->close();
    bluetooth_socket.destroy();  // Avoid leaks.
    return typename BasePCPHandler<Platform>::ConnectImplResult(
        proto::connections::Medium::BLUETOOTH, Status::ERROR);
  }

  // TODO(tracyzhou): Add logging.
  return typename BasePCPHandler<Platform>::ConnectImplResult(
      scoped_bluetooth_endpoint_channel.release());
}

template <typename Platform>
typename BasePCPHandler<Platform>::ConnectImplResult
P2PClusterPCPHandler<Platform>::bleConnectImpl(
    Ptr<ClientProxy<Platform>> client_proxy, Ptr<BLEEndpoint> ble_endpoint) {
  Ptr<BLE_PERIPHERAL> remote_ble_peripheral = ble_endpoint->getBlePeripheral();

  Ptr<BLESocket> ble_socket = medium_manager_->connectToBlePeripheral(
      remote_ble_peripheral, ble_endpoint->getServiceId());

  if (ble_socket.isNull()) {
    return typename BasePCPHandler<Platform>::ConnectImplResult(
        proto::connections::Medium::BLE, Status::BLUETOOTH_ERROR);
  }

  ScopedPtr<Ptr<EndpointChannel>> scoped_ble_endpoint_channel(
      this->endpoint_channel_manager_->createOutgoingBLEEndpointChannel(
          ble_endpoint->getEndpointId(), ble_socket));

  if (scoped_ble_endpoint_channel.isNull()) {
    ble_socket->close();
    ble_socket.destroy();  // Avoid leaks.
    return typename BasePCPHandler<Platform>::ConnectImplResult(
        proto::connections::Medium::BLE, Status::ERROR);
  }

  // TODO(tracyzhou): Add logging.
  return typename BasePCPHandler<Platform>::ConnectImplResult(
      scoped_ble_endpoint_channel.release());
}

template <typename Platform>
string P2PClusterPCPHandler<Platform>::getBlePeripheralId(
    Ptr<BLE_PERIPHERAL> ble_peripheral) {
#if BLE_V2_IMPLEMENTED
  return string(ble_peripheral->getId()->getData(),
                ble_peripheral->getId()->size());
#else
  return ble_peripheral->getBluetoothDevice()->getName();
#endif
}

template <typename Platform>
typename BasePCPHandler<Platform>::ConnectImplResult
P2PClusterPCPHandler<Platform>::WifiLanConnectImpl(
    Ptr<ClientProxy<Platform>> client_proxy,
    Ptr<WifiLanEndpoint> wifi_lan_endpoint) {
  Ptr<WifiLanService> remote_wifi_lan_service =
      wifi_lan_endpoint->GetWifiLanService();

  Ptr<WifiLanSocket> wifi_lan_socket = medium_manager_->ConnectToWifiLanService(
      remote_wifi_lan_service, wifi_lan_endpoint->getServiceId());

  if (wifi_lan_socket.isNull()) {
    return typename BasePCPHandler<Platform>::ConnectImplResult(
        proto::connections::Medium::WIFI_LAN, Status::BLUETOOTH_ERROR);
  }

  ScopedPtr<Ptr<EndpointChannel>> scoped_wifi_lan_endpoint_channel(
      this->endpoint_channel_manager_->CreateOutgoingWifiLanEndpointChannel(
          wifi_lan_endpoint->getEndpointId(), wifi_lan_socket));

  if (scoped_wifi_lan_endpoint_channel.isNull()) {
    wifi_lan_socket->Close();
    wifi_lan_socket.destroy();  // Avoid leaks.
    return typename BasePCPHandler<Platform>::ConnectImplResult(
        proto::connections::Medium::WIFI_LAN, Status::ERROR);
  }

  // TODO(b/149806065): Add logging.
  return typename BasePCPHandler<Platform>::ConnectImplResult(
      scoped_wifi_lan_endpoint_channel.release());
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
