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

#include "core/internal/mediums/ble.h"
#include "core/internal/mediums/ble_advertisement_header.h"
#include "core/internal/mediums/bloom_filter.h"
#include "core/internal/mediums/utils.h"
#include "core/internal/mediums/uuid.h"
#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

namespace ble_v2 {

template <typename Platform>
class ProcessOnLostRunnable : public Runnable {
 public:
  explicit ProcessOnLostRunnable(Ptr<BLEV2<Platform>> ble_v2)
      : ble_v2_(ble_v2) {}

  void run() override { ble_v2_->processOnLostTimeout(); }

 private:
  Ptr<BLEV2<Platform>> ble_v2_;
};

template <typename Platform>
class OnAdvertisementFoundRunnable : public Runnable {
 public:
  OnAdvertisementFoundRunnable(
      Ptr<BLEV2<Platform>> ble_v2, Ptr<BLEPeripheralV2> peripheral,
      ConstPtr<BLEAdvertisementData> advertisement_data)
      : ble_v2_(ble_v2),
        peripheral_(peripheral),
        advertisement_data_(advertisement_data) {}

  // This method is synchronized because it affects class state, but is called
  // from a separate thread that fires whenever a BLE advertisement is seen.
  void run() override {
    Synchronized s(ble_v2_->lock_.get());

    ble_v2_->discovered_peripheral_tracker_->processFoundBleAdvertisement(
        peripheral_, advertisement_data_.release(),
        MakePtr(new typename BLEV2<Platform>::GATTAdvertisementFetcherFacade(
            ble_v2_)));
  }

 private:
  Ptr<BLEV2<Platform>> ble_v2_;
  Ptr<BLEPeripheralV2> peripheral_;
  ScopedPtr<ConstPtr<BLEAdvertisementData>> advertisement_data_;
};

}  // namespace ble_v2

template <typename Platform>
const std::int32_t BLEV2<Platform>::kNumAdvertisementSlots = 2;

template <typename Platform>
const std::int32_t BLEV2<Platform>::kMaxAdvertisementLength = 512;

template <typename Platform>
const std::int32_t BLEV2<Platform>::kDummyServiceIdLength = 512;

template <typename Platform>
const char* BLEV2<Platform>::kCopresenceServiceUuid =
    "0000FEF3-0000-1000-8000-00805F9B34FB";

template <typename Platform>
const std::int64_t BLEV2<Platform>::kOnLostTimeoutMillis = 15000;

template <typename Platform>
const std::int64_t BLEV2<Platform>::kGattAdvertisementOperationTimeoutMillis =
    5000;

template <typename Platform>
const std::int64_t
    BLEV2<Platform>::kMinConnectionAttemptRecoveryDurationMillis = 1000;

template <typename Platform>
const std::int32_t
    BLEV2<Platform>::kMaxConnectionAttemptRecoveryFuzzDurationMillis = 10000;

template <typename Platform>
const std::uint32_t BLEV2<Platform>::kDefaultMtu = 512;

// These two values make up the base UUID we use when advertising a slot. The
// base is an all zero Version-3 name-based UUID. To turn this into an
// advertisement slot UUID, we simply OR the least significant bits with the
// slot number.
//
// More info about the format can be found here:
// https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based)
template <typename Platform>
const std::int64_t BLEV2<Platform>::kAdvertisementUuidMsb = 0x0000000000003000;

template <typename Platform>
const std::int64_t BLEV2<Platform>::kAdvertisementUuidLsb = 0x8000000000000000;

template <typename Platform>
BLEV2<Platform>::BLEV2(Ptr<BluetoothRadio<Platform>> bluetooth_radio)
    : lock_(Platform::createLock()),
      platform_thread_offloader_(Platform::createSingleThreadExecutor()),
      prng_(MakePtr(new Prng())),
      hash_utils_(Platform::createHashUtils()),
      bluetooth_radio_(bluetooth_radio),
      bluetooth_adapter_(Platform::createBluetoothAdapter()),
      ble_medium_(Platform::createBLEMediumV2()),
      scanning_info_(),
      discovered_peripheral_tracker_(
          new DiscoveredPeripheralTracker<Platform>()),
      on_lost_executor_(Platform::createScheduledExecutor()),
      advertising_info_(),
      gatt_server_info_(),
      accepting_connections_info_() {}

template <typename Platform>
BLEV2<Platform>::~BLEV2() {
  Synchronized s(lock_.get());

  on_lost_executor_->shutdown();
  platform_thread_offloader_->shutdown();
  stopAdvertising();
  stopAdvertisementGattServer();
  stopAcceptingConnections();
  stopScanning();
  // discovered_peripheral_tracker is a ScopedPtr member and will take care of
  // itself.
}

template <typename Platform>
bool BLEV2<Platform>::isAvailable() {
  // This is purposefully left un-synchronized like its java counterpart.
  // Callers should be able to query this without waiting for other operations
  // to complete first and this should be safe to call after shutdown. We would
  // have made it static, but it relies on variables from the constructor (like
  // ble_medium_ and bluetooth_adapter_).
  return !ble_medium_.isNull() && !bluetooth_adapter_.isNull();
}

// Returns true if currently scanning for BLE advertisements.
template <typename Platform>
bool BLEV2<Platform>::isAdvertising() {
  Synchronized s(lock_.get());

  return !advertising_info_.isNull();
}

// Starts BLE advertising, delivering additional information through a GATT
// server.
template <typename Platform>
bool BLEV2<Platform>::startAdvertising(
    const string& service_id, ConstPtr<ByteArray> advertisement_bytes,
    BLEMediumV2::PowerMode::Value power_mode,
    const string& fast_advertisement_service_uuid) {
  Synchronized s(lock_.get());

  // Avoid leaks.
  ScopedPtr<ConstPtr<ByteArray>> scoped_advertisement_bytes(
      advertisement_bytes);

  if (service_id.empty() || scoped_advertisement_bytes.isNull()) {
    // logger.atSevere().log("Refusing to start BLE advertising because a null
    // parameter was passed in.");
    return false;
  }

  if (scoped_advertisement_bytes->size() > kMaxAdvertisementLength) {
    // logger.atSevere().log("Refusing to start BLE advertising because the
    // advertisement was too long. Expected at most %d bytes but received %d.",
    // kMaxAdvertisementLength, scoped_advertisement_bytes->size());
    return false;
  }

  // Note: We don't include logic checking/using the fast_pair_model_id because
  // that is a java-only concept for now.

  if (isAdvertising()) {
    // logger.atSevere().log("Failed to BLE advertise because we're already
    // advertising.");
    return false;
  }

  if (!bluetooth_radio_->isEnabled()) {
    // logger.atSevere().log("Can't start BLE advertising because Bluetooth
    // isn't enabled.");
    return false;
  }

  if (!isAvailable()) {
    // logger.atSevere().log("Can't start BLE advertising because BLE is not
    // available.");
    return false;
  }

  // TODO(ahlee): Remove this check here and in the java code (redundant)
  // Stop any existing advertisement GATT servers. We don't stop it in
  // stopAdvertising() to avoid GATT issues with BLE sockets.
  if (isAdvertisementGattServerRunning()) {
    stopAdvertisementGattServer();
  }

  // Start a GATT server to deliver the full advertisement data. If we fail to
  // advertise the header, we must shut this down before the method returns.
  bool is_fast_advertisement = !fast_advertisement_service_uuid.empty();
  if (!is_fast_advertisement) {
    if (!startAdvertisementGattServer(service_id,
                                      scoped_advertisement_bytes.get())) {
      // logger.atSevere().log("Failed to to BLE advertise because the
      // advertisement GATT server failed to start");
      return false;
    }
  }

  ScopedPtr<ConstPtr<ByteArray>> advertisement_header_bytes(
      createAdvertisementHeader(service_id, scoped_advertisement_bytes.get(),
                                is_fast_advertisement));
  if (advertisement_header_bytes.isNull()) {
    // logger.atSevere().log("Failed to to BLE advertise because we could not
    // create an advertisement header");
    // We failed to start BLE advertising, so stop the advertisement GATT
    // server.
    stopAdvertisementGattServer();
    return false;
  }

  ScopedPtr<Ptr<BLEAdvertisementData>> advertisement(
      new BLEAdvertisementData());
  advertisement->is_connectable = true;
  advertisement->tx_power_level =
      BLEAdvertisementData::UNSPECIFIED_TX_POWER_LEVEL;

  ScopedPtr<Ptr<BLEAdvertisementData>> scan_response(
      new BLEAdvertisementData());
  scan_response->is_connectable = true;
  scan_response->tx_power_level =
      BLEAdvertisementData::UNSPECIFIED_TX_POWER_LEVEL;
  scan_response->service_uuids.insert(kCopresenceServiceUuid);
  scan_response->service_data.insert(std::make_pair(
      kCopresenceServiceUuid, advertisement_header_bytes.release()));

  // Note: We don't use fast pair data because that is java-only for now.

  // TODO(ahlee): Fix this if check in the java code.
  if (is_fast_advertisement) {
    ScopedPtr<ConstPtr<ByteArray>> service_id_hash(
        generateServiceIdHash(BLEAdvertisement::Version::V2, service_id));
    ScopedPtr<ConstPtr<ByteArray>> fast_advertisement_bytes(
        BLEAdvertisement::toBytes(
            BLEAdvertisement::Version::V2, BLEAdvertisement::SocketVersion::V2,
            service_id_hash.get(), scoped_advertisement_bytes.get()));
    if (fast_advertisement_bytes.isNull()) {
      // logger.atSevere().log("Failed to BLE advertise because we could not
      // create a fast advertisement for service UUID %s.",
      // fast_advertisement_service_uuid);

      // We shouldn't have started an advertisement GATT server in the first
      // place if we are using fast advertisements. However, to avoid careless
      // leaks, try shutting down the server anyway.
      stopAdvertisementGattServer();
      return false;
    }
    advertisement->service_data.insert(std::make_pair(
        fast_advertisement_service_uuid, fast_advertisement_bytes.release()));
    scan_response->service_uuids.insert(fast_advertisement_service_uuid);
  }

  if (!ble_medium_->startAdvertising(ConstifyPtr(advertisement.release()),
                                     ConstifyPtr(scan_response.release()),
                                     power_mode)) {
    // If BLE advertising was not successful, stop the advertisement GATT
    // server.
    stopAdvertisementGattServer();
    return false;
  }

  // logger.atVerbose().flog("Started BLE advertising with advertisement %s for
  // serviceID %s.", advertisement_header, service_id);
  advertising_info_ = MakePtr(new AdvertisingInfo(service_id));
  return true;
}

template <typename Platform>
ConstPtr<ByteArray> BLEV2<Platform>::createAdvertisementHeader(
    const string& service_id, ConstPtr<ByteArray> advertisement_bytes,
    bool is_fast_advertisement) {
  // Create a randomized dummy service ID to anonymize our header with.
  string dummy_service_id;
  dummy_service_id.reserve(kDummyServiceIdLength);
  for (int i = 0; i < kDummyServiceIdLength; i++) {
    dummy_service_id[i] = static_cast<char>(prng_->nextInt32() & 0x000000FF);
  }

  // Put the service ID along with the dummy service ID into our bloom filter
  // Note: BloomFilter length should always match
  // BLEAdvertisementHeader::kServiceIdBloomFilterLength
  ScopedPtr<Ptr<BloomFilter<10>>> bloom_filter(new BloomFilter<10>());
  bloom_filter->add(dummy_service_id);

  // Only add the service ID to our bloom filter if it's not a fast
  // advertisement. Fast advertisements want discoverers to avoid reading our
  // GATT advertisement.
  if (!is_fast_advertisement) {
    bloom_filter->add(service_id);
  }

  // Create a hash seeded from dummy_service_id + advertisementBytes
  //
  // First, populate advertisement_bodies with the dummy_service_id and
  // advertisement_bytes.
  string advertisement_bodies;
  advertisement_bodies.reserve(dummy_service_id.size() +
                               advertisement_bytes->size());
  advertisement_bodies.append(dummy_service_id.data(), dummy_service_id.size());
  advertisement_bodies.append(advertisement_bytes->getData(),
                              advertisement_bytes->size());

  // Then, generate the advertisement hash from the populated
  // advertisement_bodies string.
  ScopedPtr<ConstPtr<ByteArray>> advertisement_bodies_byte_array(MakeConstPtr(
      new ByteArray(advertisement_bodies.data(), advertisement_bodies.size())));
  ScopedPtr<ConstPtr<ByteArray>> advertisement_hash(
      generateAdvertisementHash(advertisement_bodies_byte_array.get()));

  ScopedPtr<ConstPtr<ByteArray>> bloom_filter_bytes(bloom_filter->asBytes());
  string ble_advertisement_header_string = BLEAdvertisementHeader::asString(
      BLEAdvertisementHeader::Version::V2, kNumAdvertisementSlots,
      bloom_filter_bytes.get(), advertisement_hash.get());

  return MakeConstPtr(new ByteArray(ble_advertisement_header_string.data(),
                                    ble_advertisement_header_string.size()));
}

// Stops BLE advertising.
template <typename Platform>
void BLEV2<Platform>::stopAdvertising() {
  Synchronized s(lock_.get());

  if (!isAdvertising()) {
    // logger.atDebug().log("Can't turn off BLE advertising because it never
    // started.");
    return;
  }

  ble_medium_->stopAdvertising();
  // Reset advertising_info_to mark that we're no longer advertising.
  advertising_info_.destroy();

  // Do NOT stop the advertisement GATT server here. Doing so will cause any
  // other existing GATT connections to stop receiving callbacks. This affects
  // our BLE sockets. Therefore, we only stop it in shutdown() and
  // startAdvertising(), where it is safe to do so. At those two points, we
  // shouldn't expect any BLE sockets to be connected.

  // logger.atVerbose().log("Turned BLE advertising off");
}

// Returns true if currently scanning for BLE advertisements.
template <typename Platform>
bool BLEV2<Platform>::isScanning() {
  Synchronized s(lock_.get());

  return !scanning_info_.isNull();
}

// Starts scanning for BLE advertisements (if it is possible for the device).
template <typename Platform>
bool BLEV2<Platform>::startScanning(
    const string& service_id,
    Ptr<DiscoveredPeripheralCallback> discovered_peripheral_callback,
    BLEMediumV2::PowerMode::Value power_mode,
    const string& fast_advertisement_service_uuid) {
  Synchronized s(lock_.get());

  // Avoid leaks.
  ScopedPtr<Ptr<DiscoveredPeripheralCallback>>
      scoped_discovered_peripheral_callback(discovered_peripheral_callback);

  if (service_id.empty() || scoped_discovered_peripheral_callback.isNull()) {
    // logger.atSevere().log("Refusing to start BLE scanning because at least
    // one of workSource, serviceId, or discoveredPeripheralCallback is null.");
    return false;
  }

  if (isScanning()) {
    // logger.atSevere().log("Refusing to start BLE scanning because we are
    // already scanning.");
    return false;
  }

  if (!bluetooth_radio_->isEnabled()) {
    // logger.atSevere().log("Can't start BLE scanning because Bluetooth was
    // never turned on");
    return false;
  }

  if (!isAvailable()) {
    // logger.atSevere().log("Can't start BLE scanning because BLE is not
    // available.");
    return false;
  }

  discovered_peripheral_tracker_->startTracking(
      service_id, scoped_discovered_peripheral_callback.release(),
      fast_advertisement_service_uuid);
  // Avoid leaks.
  ScopedPtr<Ptr<ScanCallbackFacade>> scan_callback_facade(
      new ScanCallbackFacade(self_));
  std::set<string> service_uuids;
  service_uuids.insert(kCopresenceServiceUuid);
  if (!ble_medium_->startScanning(service_uuids, power_mode,
                                  scan_callback_facade.get())) {
    discovered_peripheral_tracker_->stopTracking(service_id);
    return false;
  }

  // logger.atVerbose().log("Started BLE scanning for serviceID %s.",
  // service_id);
  scanning_info_ = MakePtr(new ScanningInfo(
      service_id, scan_callback_facade.release(), createOnLostAlarm()));
  return true;
}

template <typename Platform>
void BLEV2<Platform>::onAdvertisementFoundImpl(
    Ptr<BLEPeripheralV2> ble_peripheral,
    ConstPtr<BLEAdvertisementData> advertisement_data) {
  offloadFromPlatformThread(
      MakePtr(new ble_v2::OnAdvertisementFoundRunnable<Platform>(
          self_, ble_peripheral, advertisement_data)));
}

// This method is synchronized because it affects class state, but is called
// from a separate thread that has a recurring alarm running on it.
template <typename Platform>
void BLEV2<Platform>::processOnLostTimeout() {
  Synchronized s(lock_.get());

  discovered_peripheral_tracker_->processLostGattAdvertisements();
}

// Stops scanning for BLE advertisements.
template <typename Platform>
void BLEV2<Platform>::stopScanning() {
  Synchronized s(lock_.get());

  if (!isScanning()) {
    // logger.atDebug().log("Can't turn off BLE scanning because we never
    // started scanning.");
    return;
  }

  scanning_info_->on_lost_alarm->cancel();

  ble_medium_->stopScanning();
  discovered_peripheral_tracker_->stopTracking(scanning_info_->service_id);
  // Reset our bundle of scanning state to mark that we're no longer scanning.
  scanning_info_.destroy();
}

// TODO(b/112199086) Change to RecurringCancelableAlarm
template <typename Platform>
Ptr<CancelableAlarm> BLEV2<Platform>::createOnLostAlarm() {
  return Ptr<CancelableAlarm>();
}

// Returns true if the device is currently accepting incoming BLE socket
// connections.
template <typename Platform>
bool BLEV2<Platform>::isAcceptingConnections() {
  Synchronized s(lock_.get());

  return !accepting_connections_info_.isNull();
}

// Starts accepting incoming BLE socket connections.
template <typename Platform>
bool BLEV2<Platform>::startAcceptingConnections(
    const string& service_id,
    Ptr<AcceptedConnectionCallback> accepted_connection_callback) {
  Synchronized s(lock_.get());

  // Avoid leaks.
  ScopedPtr<Ptr<AcceptedConnectionCallback>>
      scoped_accepted_connection_callback(accepted_connection_callback);
  if (service_id.empty() || scoped_accepted_connection_callback.isNull()) {
    // logger.atSevere().log("Refusing to start accepting BLE connections
    // because at least one of serviceId or acceptedConnectionCallback is
    // null.");
    return false;
  }

  if (isAcceptingConnections()) {
    // logger.atSevere().log("Refusing to start accepting BLE connections for %s
    // because another BLE server socket is already in-progress.", service_id);
    return false;
  }

  if (!bluetooth_radio_->isEnabled()) {
    // logger.atSevere().log("Can't start accepting BLE connections for %s
    // because Bluetooth isn't enabled.", service_id);
    return false;
  }

  if (!isAvailable()) {
    // logger.atSevere().log("Can't start accepting BLE connections for %s
    // because BLE is not available.", service_id);
    return false;
  }

  // TODO(ahlee): Implement w/ the rest of the connecting logic.
  // Default to returning true and creating accepting_connections_info_ so we
  // can test the advertising and discovery flow fully.
  accepting_connections_info_ =
      MakePtr(new AcceptingConnectionsInfo(service_id));
  return true;
}

// Stops accepting incoming BLE socket connections.
template <typename Platform>
void BLEV2<Platform>::stopAcceptingConnections() {
  Synchronized s(lock_.get());

  if (!isAcceptingConnections()) {
    // logger.atDebug().log("Can't stop accepting BLE connections because it was
    // never started.");
    return;
  }

  ble_medium_->stopListeningForIncomingBLESockets();

  // Reset our bundle of accepting connections state to mark that we're no
  // longer accepting connections.
  accepting_connections_info_.destroy();
}

// Note: getGattConnectionBackoffPeriodMillis is only used in the java version
// of reliablyConnect() for now.

// Returns true if the advertisement GATT server is currently running.
template <typename Platform>
bool BLEV2<Platform>::isAdvertisementGattServerRunning() {
  return !gatt_server_info_.isNull();
}

// Starts a GATT server to deliver additional advertisement data. Returns true
// if the server was started successfully.
template <typename Platform>
bool BLEV2<Platform>::startAdvertisementGattServer(
    const string& service_id, ConstPtr<ByteArray> advertisement) {
  // advertisement is not being wrapped in a ScopedPtr because ownership is not
  // passed on from startAdvertising().

  if (isAdvertisementGattServerRunning()) {
    // logger.atSevere().log("Refusing to start an advertisement GATT server
    // because one is already running.");
    return false;
  }

  // Create a BleAdvertisement to wrap over the passed in advertisement.
  ScopedPtr<ConstPtr<ByteArray>> legacy_service_id_hash(
      generateServiceIdHash(BLEAdvertisement::Version::V1, service_id));
  ScopedPtr<ConstPtr<ByteArray>> legacy_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(BLEAdvertisement::Version::V1,
                                BLEAdvertisement::SocketVersion::V1,
                                legacy_service_id_hash.get(), advertisement));
  if (legacy_ble_advertisement_bytes.isNull()) {
    // logger.atSevere().log("Refusing to start an advertisement GATT server
    // because creating a legacy BleAdvertisement with service ID %s failed.",
    // service_id);
    return false;
  }

  ScopedPtr<ConstPtr<ByteArray>> service_id_hash(
      generateServiceIdHash(BLEAdvertisement::Version::V2, service_id));
  ScopedPtr<ConstPtr<ByteArray>> ble_advertisement_bytes(
      BLEAdvertisement::toBytes(BLEAdvertisement::Version::V2,
                                BLEAdvertisement::SocketVersion::V2,
                                service_id_hash.get(), advertisement));
  if (ble_advertisement_bytes.isNull()) {
    // logger.atSevere().log("Refusing to start an advertisement GATT server
    // because creating a BleAdvertisement with service ID %s failed.",
    // service_id);
    return false;
  }

  return internalStartAdvertisementGattServer(
      legacy_ble_advertisement_bytes.release(),
      ble_advertisement_bytes.release());
}

template <typename Platform>
bool BLEV2<Platform>::internalStartAdvertisementGattServer(
    ConstPtr<ByteArray> legacy_ble_advertisement_bytes,
    ConstPtr<ByteArray> ble_advertisement_bytes) {
  // Avoid leaks.
  ScopedPtr<ConstPtr<ByteArray>> scoped_legacy_ble_advertisement_bytes(
      legacy_ble_advertisement_bytes);
  ScopedPtr<ConstPtr<ByteArray>> scoped_ble_advertisement_bytes(
      ble_advertisement_bytes);

  ScopedPtr<Ptr<ServerGATTConnectionLifecycleCallbackFacade>>
      connection_lifecycle_callback(
          new ServerGATTConnectionLifecycleCallbackFacade(self_));
  ScopedPtr<Ptr<GATTServer>> gatt_server(
      ble_medium_->startGATTServer(connection_lifecycle_callback.get()));
  if (gatt_server.isNull()) {
    // logger.atSevere().withCause(e).log("Unable to start an advertisement GATT
    // server.");
    return false;
  }

  if (!generateAdvertisementCharacteristic(
          /* slot= */ 0, scoped_legacy_ble_advertisement_bytes.release(),
          gatt_server.get())) {
    gatt_server->stop();
    return false;
  }

  if (!generateAdvertisementCharacteristic(
          /* slot= */ 1, scoped_ble_advertisement_bytes.release(),
          gatt_server.get())) {
    gatt_server->stop();
    return false;
  }

  // GattCharacteristic is not included in GATTServerInfo because we don't need
  // it after it's been updated.
  gatt_server_info_ = MakePtr(new GATTServerInfo(
      gatt_server.release(), connection_lifecycle_callback.release()));
  return true;
}

template <typename Platform>
bool BLEV2<Platform>::generateAdvertisementCharacteristic(
    std::int32_t slot, ConstPtr<ByteArray> advertisement,
    Ptr<GATTServer> gatt_server) {
  // Avoid leaks.
  ScopedPtr<ConstPtr<ByteArray>> scoped_advertisement(advertisement);

  std::set<GATTCharacteristic::Permission::Value> permissions;
  permissions.insert(GATTCharacteristic::Permission::READ);
  std::set<GATTCharacteristic::Property::Value> properties;
  properties.insert(GATTCharacteristic::Property::READ);
  Ptr<GATTCharacteristic> gatt_characteristic(gatt_server->createCharacteristic(
      kCopresenceServiceUuid, generateAdvertisementUuid(slot), permissions,
      properties));

  if (gatt_characteristic.isNull()) {
    // logger.atSevere().withCause(e).log("Unable to create and add a
    // characterstic to the gatt server for the advertisement.");
    return false;
  }

  if (!gatt_server->updateCharacteristic(gatt_characteristic,
                                         scoped_advertisement.release())) {
    // logger.atSevere().withCause(e).log("Unable to write a value to the GATT
    // characteristic.");
    return false;
  }

  return true;
}

// Note: In the java counterpart this in a utils class.
// Generates a characteristic UUID for an advertisement at the given slot.
template <typename Platform>
string BLEV2<Platform>::generateAdvertisementUuid(std::int32_t slot) {
  return UUID<Platform>(kAdvertisementUuidMsb, kAdvertisementUuidLsb | slot)
      .str();
}

// Stops a GATT server used for additional advertisement data.
template <typename Platform>
void BLEV2<Platform>::stopAdvertisementGattServer() {
  Synchronized s(lock_.get());

  if (!isAdvertisementGattServerRunning()) {
    // logger.atSevere().log("Unable to stop the advertisement GATT server
    // because it's not running.");
    return;
  }

  gatt_server_info_->gatt_server->stop();
  gatt_server_info_.destroy();
}

// Connects to a GATT server, reads advertisement data, and then disconnects
// from the GATT server. This method blocks until all advertisements are read,
// or a connection error occurs.
template <typename Platform>
Ptr<AdvertisementReadResult<Platform>>
BLEV2<Platform>::processFetchGattAdvertisementsRequest(
    Ptr<BLEPeripheralV2> peripheral, std::int32_t num_slots,
    Ptr<AdvertisementReadResult<Platform>> advertisement_read_result) {
  Synchronized s(lock_.get());

  if (advertisement_read_result.isNull()) {
    advertisement_read_result =
        MakeRefCountedPtr(new AdvertisementReadResult<Platform>());
  }

  if (peripheral.isNull()) {
    // logger.atSevere().log("Can't read from an advertisement GATT server
    // because ble peripheral is null.");
    return advertisement_read_result;
  }

  if (!bluetooth_radio_->isEnabled()) {
    // logger.atSevere().log("Can't read from an advertisement GATT server
    // because Bluetooth was never turned on.");
    return advertisement_read_result;
  }

  if (!isAvailable()) {
    // logger.atSevere().log("Can't read from an advertisement GATT server
    // because BLE is not available.");
    return advertisement_read_result;
  }

  return internalReadFromAdvertisementGattServer(peripheral, num_slots,
                                                 advertisement_read_result);
}

template <typename Platform>
Ptr<AdvertisementReadResult<Platform>>
BLEV2<Platform>::internalReadFromAdvertisementGattServer(
    Ptr<BLEPeripheralV2> peripheral, std::int32_t num_slots,
    Ptr<AdvertisementReadResult<Platform>> advertisement_read_result) {
  // Attempt to connect and read some GATT characteristics.
  bool read_success = true;

  ScopedPtr<Ptr<ClientGATTConnectionLifecycleCallbackFacade>>
      connection_lifecycle_callback(
          new ClientGATTConnectionLifecycleCallbackFacade(self_));
  ScopedPtr<Ptr<ClientGATTConnection>> gatt_connection(
      ble_medium_->connectToGATTServer(peripheral, kDefaultMtu,
                                       BLEMediumV2::PowerMode::HIGH,
                                       connection_lifecycle_callback.get()));
  if (!gatt_connection.isNull() && gatt_connection->discoverServices()) {
    // Read all advertisements from all slots that we haven't read from yet.
    for (std::int32_t slot = 0; slot < num_slots; ++slot) {
      // Make sure we haven't already read this advertisement before.
      if (advertisement_read_result->hasAdvertisement(slot)) {
        continue;
      }

      // Make sure the characteristic even exists for this slot number. If the
      // characteristic doesn't exist, we shouldn't count the fetch as a
      // failure because there's nothing we could've done about a non-existent
      // characteristic.
      Ptr<GATTCharacteristic> gatt_characteristic(
          gatt_connection->getCharacteristic(kCopresenceServiceUuid,
                                             generateAdvertisementUuid(slot)));
      if (/* !advertisementSlotExists()= */ gatt_characteristic.isNull()) {
        continue;
      }

      // Read advertisement data from the characteristic associated with this
      // slot.
      ScopedPtr<ConstPtr<ByteArray>> characteristic_value(
          gatt_connection->readCharacteristic(gatt_characteristic));
      if (!characteristic_value.isNull()) {
        advertisement_read_result->addAdvertisement(
            slot, characteristic_value.release());
        // logger.atVerbose().log("Successfully read advertisement at slot %d
        // on peripheral %s.", slot, peripheral);
      } else {
        // logger.atWarning().withCause(characteristicReadException).log("Can't
        // read advertisement for slot %d on peripheral %s.", slot,
        // peripheral);
        read_success = false;
      }
      // Whether or not we succeeded with this slot, we should try reading the
      // other slots to get as many advertisements as possible before
      // returning a success or failure.
    }

    gatt_connection->disconnect();
  } else {
    // logger.atWarning().withCause(connectException).log("Can't connect to an
    // advertisement GATT server for peripheral %s.", peripheral);
    read_success = false;
  }

  advertisement_read_result->recordLastReadStatus(read_success);
  return advertisement_read_result;
}

template <typename Platform>
void BLEV2<Platform>::offloadFromPlatformThread(Ptr<Runnable> runnable) {
  platform_thread_offloader_->execute(runnable);
}

template <typename Platform>
ConstPtr<ByteArray> BLEV2<Platform>::generateAdvertisementHash(
    ConstPtr<ByteArray> advertisement_bytes) {
  return Utils::sha256Hash(hash_utils_.get(), advertisement_bytes,
                           BLEAdvertisementHeader::kAdvertisementHashLength);
}

template <typename Platform>
ConstPtr<ByteArray> BLEV2<Platform>::generateServiceIdHash(
    BLEAdvertisement::Version::Value version, const string& service_id) {
  ScopedPtr<ConstPtr<ByteArray>> service_id_bytes(
      MakeConstPtr(new ByteArray(service_id.data(), service_id.size())));
  switch (version) {
    case BLEAdvertisement::Version::V1:
      return Utils::legacySha256HashOnlyForPrinting(
          hash_utils_.get(), service_id_bytes.get(),
          BLEAdvertisement::kServiceIdHashLength);
    case BLEAdvertisement::Version::V2:
      // Fall through.
    case BLEAdvertisement::Version::UNKNOWN:
      // Fall through.
    default:
      // Use the latest known hashing scheme.
      return Utils::sha256Hash(hash_utils_.get(), service_id_bytes.get(),
                               BLEAdvertisement::kServiceIdHashLength);
  }
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
