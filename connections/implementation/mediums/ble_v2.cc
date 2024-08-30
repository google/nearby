// Copyright 2022 Google LLC
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

#include "connections/implementation/mediums/ble_v2.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/ble_v2/advertisement_read_result.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble_v2/ble_utils.h"
#include "connections/implementation/mediums/ble_v2/bloom_filter.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_tracker.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/power_level.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/runnable.h"
#include "internal/platform/uuid.h"

namespace nearby {
namespace connections {

namespace {

using ::nearby::api::ble_v2::BleAdvertisementData;
using ::nearby::api::ble_v2::GattCharacteristic;
using ::nearby::api::ble_v2::TxPowerLevel;

constexpr int kMaxAdvertisementLength = 512;
constexpr int kDummyServiceIdLength = 128;

// Tell the thread annotation static analysis that `m` is already exclusively
// locked. Used because the analysis is not interprocedural.
void AssumeHeld(Mutex& m) ABSL_ASSERT_EXCLUSIVE_LOCK(m) {}

}  // namespace

BleV2::BleV2(BluetoothRadio& radio)
    : radio_(radio), adapter_(radio_.GetBluetoothAdapter()) {}

BleV2::~BleV2() {
  // Destructor is not taking locks, but methods it is calling are.
  if (FeatureFlags::GetInstance().GetFlags().enable_ble_v2_async_scanning) {
    // If using asynchronous scanning, check the corresponding map.
    while (!service_ids_to_scanning_sessions_.empty()) {
      StopScanning(service_ids_to_scanning_sessions_.begin()->first);
    }
  } else {
    while (!scanned_service_ids_.empty()) {
      StopScanning(*scanned_service_ids_.begin());
    }
  }
  while (!advertising_infos_.empty()) {
    StopAdvertising(advertising_infos_.begin()->first);
  }
  while (!server_sockets_.empty()) {
    StopAcceptingConnections(server_sockets_.begin()->first);
  }

  serial_executor_.Shutdown();
  alarm_executor_.Shutdown();
  accept_loops_runner_.Shutdown();

  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnableInstantOnLost)) {
    instant_on_lost_manager_.Shutdown();
  }
}

bool BleV2::IsAvailable() const {
  MutexLock lock(&mutex_);

  return IsAvailableLocked();
}

bool BleV2::StartAdvertising(const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             PowerLevel power_level,
                             bool is_fast_advertisement) {
  MutexLock lock(&mutex_);

  if (advertisement_bytes.Empty()) {
    NEARBY_LOGS(INFO)
        << "Refusing to turn on BLE advertising. Empty advertisement data.";
    return false;
  }

  if (advertisement_bytes.size() > kMaxAdvertisementLength) {
    NEARBY_LOGS(INFO) << "Refusing to start BLE advertising because the "
                         "advertisement was too long. Expected at most "
                      << kMaxAdvertisementLength << " bytes but received "
                      << advertisement_bytes.size() << " bytes.";
    return false;
  }

  if (IsAdvertisingLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Failed to BLE advertise because we're already advertising.";
    return false;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO)
        << "Can't start BLE advertising because Bluetooth was never turned on";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't turn on BLE advertising. BLE is not available.";
    return false;
  }

  // Wrap the connections advertisement to the medium advertisement.
  ByteArray service_id_hash = mediums::bleutils::GenerateHash(
      service_id, mediums::BleAdvertisement::kServiceIdHashLength);
  // Get psm value from L2CAP server if L2CAP is supported. Now just use the
  // default value.
  int psm = mediums::BleAdvertisementHeader::kDefaultPsmValue;
  mediums::BleAdvertisement medium_advertisement = {
      mediums::BleAdvertisement::Version::kV2,
      mediums::BleAdvertisement::SocketVersion::kV2,
      /*service_id_hash=*/is_fast_advertisement ? ByteArray{} : service_id_hash,
      advertisement_bytes,
      mediums::bleutils::GenerateDeviceToken(),
      psm};
  if (!medium_advertisement.IsValid()) {
    NEARBY_LOGS(INFO) << "Failed to BLE advertise because we could not wrap a "
                         "connection advertisement to medium advertisement.";
    return false;
  }

  advertising_infos_.insert(
      {service_id,
       AdvertisingInfo{.medium_advertisement = medium_advertisement,
                       .power_level = power_level,
                       .is_fast_advertisement = is_fast_advertisement}});

  // TODO(hais): need to update here after cros support RAII StartAdvertising.
  // After all platforms support RAII StartAdvertising, then we can stop
  // advertising operations precisely without affect other advertising sessions.
  // Currently, cros RAII StartAdvertising is not there yet. And the advertising
  // for legacy device will be stopped from later StartAdvertising by this line
  // below. Comment it out now.
  // medium_.StopAdvertising();

  if (!StartAdvertisingLocked(service_id)) {
    advertising_infos_.erase(service_id);
    return false;
  }
  return true;
}

bool BleV2::StopAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);
  if (!IsAdvertisingLocked(service_id)) {
    NEARBY_LOGS(INFO) << "Cannot stop BLE advertising for service_id="
                      << service_id << " because it never started.";
    return false;
  }

  // Stop the BLE advertisement. We will restart it later if necessary.
  NEARBY_LOGS(INFO) << "Turned off BLE advertising with service_id="
                    << service_id;
  advertising_infos_.erase(service_id);
  medium_.StopAdvertising();

  // Remove the GATT advertisements.
  gatt_advertisements_.clear();

  // Restart the BLE advertisement if there is still an advertiser.
  if (!advertising_infos_.empty()) {
    if (!hosted_gatt_characteristics_.empty()) {
      // Set the value of characteristic to empty if there is still an
      // advertiser.
      ByteArray empty_value = {};
      for (const auto& characteristic : hosted_gatt_characteristics_) {
        if (!gatt_server_->UpdateCharacteristic(characteristic, empty_value)) {
          NEARBY_LOGS(ERROR)
              << "Failed to clear characteristic uuid="
              << std::string(characteristic.uuid)
              << " after stopping BLE advertisement for service_id="
              << service_id;
        }
      }
      hosted_gatt_characteristics_.clear();
    }
    const std::string& new_service_id = advertising_infos_.begin()->first;
    if (!StartAdvertisingLocked(new_service_id)) {
      NEARBY_LOGS(ERROR)
          << "Failed to restart BLE advertisement after stopping "
             "BLE advertisement for new service_id="
          << new_service_id;
      advertising_infos_.erase(new_service_id);
      return false;
    }
    NEARBY_LOGS(INFO) << "Restart BLE advertising with new service_id="
                      << new_service_id;
  } else if (incoming_sockets_.empty()) {
    // Otherwise, if we aren't restarting the BLE advertisement, then shutdown
    // the gatt server if it's not in use.
    NEARBY_VLOG(1) << "Aggressively stopping any pre-existing "
                      "advertisement GATT servers "
                      "because no incoming BLE sockets are connected.";
    StopAdvertisementGattServerLocked();
  }

  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnableInstantOnLost)) {
    instant_on_lost_manager_.OnAdvertisingStopped(service_id);
  }

  return true;
}

bool BleV2::IsAdvertising(const std::string& service_id) const {
  MutexLock lock(&mutex_);

  return IsAdvertisingLocked(service_id);
}

bool BleV2::IsAdvertisingForLegacyDevice(const std::string& service_id) const {
  MutexLock lock(&mutex_);

  return IsAdvertisingForLegacyDeviceLocked(service_id);
}

bool BleV2::StartLegacyAdvertising(
    const std::string& input_service_id, const std::string& local_endpoint_id,
    const std::string& fast_advertisement_service_uuid) {
  NEARBY_LOGS(INFO) << "StartLegacyAdvertising: " << input_service_id.c_str()
                    << ", local_endpoint_id: " << local_endpoint_id.c_str();
  MutexLock lock(&mutex_);

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO) << "Can't start BLE v2 legacy advertising because "
                         "Bluetooth was never turned on";
    return false;
  }
  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO)
        << "Can't turn on BLE v2 legacy advertising. BLE is not available.";
    return false;
  }
  if (medium_.IsExtendedAdvertisementsAvailable()) {
    NEARBY_LOGS(INFO) << "Skip dummy advertising for non legacy device";
    return true;
  }
  std::string service_id = input_service_id + "-Legacy";
  if (service_ids_to_advertising_sessions_.find(service_id) !=
      service_ids_to_advertising_sessions_.end()) {
    NEARBY_LOGS(INFO) << "Already started legacy device advertising for "
                      << service_id;
    return false;
  }

  std::unique_ptr<api::ble_v2::BleMedium::AdvertisingSession>
      legacy_device_advertizing_session = medium_.StartAdvertising(
          CreateAdvertisingDataForLegacyDevice(),
          {.tx_power_level = TxPowerLevel::kMedium, .is_connectable = true},
          api::ble_v2::BleMedium::AdvertisingCallback{
              .start_advertising_result =
                  [this, &service_id](absl::Status status) mutable {
                    AssumeHeld(mutex_);
                    if (status.ok()) {
                      NEARBY_LOGS(INFO)
                          << "BLE V2 advertising for legacy device started "
                             "successfully for service ID "
                          << &service_id;
                    } else {
                      NEARBY_LOGS(ERROR) << "BLE V2 advertising for legacy "
                                            "device failed for service ID "
                                         << &service_id << ": " << status;
                      service_ids_to_advertising_sessions_.erase(service_id);
                    }
                  },
          });
  if (legacy_device_advertizing_session == nullptr) {
    NEARBY_LOGS(ERROR) << "Failed to turn on BLE v2 advertising for legacy "
                          "device for service ID "
                       << service_id;
    return false;
  }

  service_ids_to_advertising_sessions_.insert(
      {std::string(service_id), std::move(legacy_device_advertizing_session)});
  return true;
}

bool BleV2::StopLegacyAdvertising(const std::string& input_service_id) {
  NEARBY_LOGS(INFO) << "StopLegacyAdvertising:" << input_service_id.c_str();
  MutexLock lock(&mutex_);

  std::string service_id = input_service_id + "-Legacy";
  auto legacy_device_advertising_session =
      service_ids_to_advertising_sessions_.find(service_id);
  if (legacy_device_advertising_session ==
      service_ids_to_advertising_sessions_.end()) {
    NEARBY_LOGS(INFO) << "Can't find session to turn off legacy device BLE "
                         "advertisingfor this service ID: "
                      << service_id;
    return false;
  }
  absl::Status status =
      legacy_device_advertising_session->second->stop_advertising();

  if (!status.ok()) {
    NEARBY_LOGS(WARNING) << "StopLegacyAdvertising error: " << status;
  }
  service_ids_to_advertising_sessions_.erase(legacy_device_advertising_session);
  NEARBY_LOGS(INFO) << "Removed advertising-session for " << service_id;
  return status.ok();
}

bool BleV2::StartScanning(const std::string& service_id, PowerLevel power_level,
                          DiscoveredPeripheralCallback callback) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO) << "Can not start BLE scanning with empty service id.";
    return false;
  }

  if (IsScanningLocked(service_id)) {
    NEARBY_LOGS(INFO) << "Cannot start scan of BLE peripherals because "
                         "scanning is already in-progress.";
    return false;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO)
        << "Can't start BLE scanning because Bluetooth is disabled";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO)
        << "Can't scan BLE peripherals because BLE isn't available.";
    return false;
  }

  // Start to track the advertisement found for specific `service_id`.
  discovered_peripheral_tracker_.StartTracking(
      service_id, std::move(callback),
      mediums::bleutils::kCopresenceServiceUuid);

  if (FeatureFlags::GetInstance().GetFlags().enable_ble_v2_async_scanning) {
    return StartAsyncScanningLocked(service_id, power_level);
  }

  // Check if scan has been activated, if yes, no need to notify client
  // to scan again.
  if (!scanned_service_ids_.empty()) {
    scanned_service_ids_.insert(service_id);
    NEARBY_LOGS(INFO) << "Turned on BLE scanning with service id=" << service_id
                      << " without start client scanning";
    return true;
  }

  scanned_service_ids_.insert(service_id);
  // TODO(b/213835576): We should re-start scanning once the power level is
  // changed.
  if (!medium_.StartScanning(
          mediums::bleutils::kCopresenceServiceUuid,
          PowerLevelToTxPowerLevel(power_level),
          {
              .advertisement_found_cb =
                  [this](BleV2Peripheral peripheral,
                         BleAdvertisementData advertisement_data) {
                    RunOnBleThread([this, peripheral = std::move(peripheral),
                                    advertisement_data]() {
                      MutexLock lock(&mutex_);
                      discovered_peripheral_tracker_
                          .ProcessFoundBleAdvertisement(
                              std::move(peripheral), advertisement_data,
                              [this](BleV2Peripheral peripheral, int num_slots,
                                     int psm,
                                     const std::vector<std::string>&
                                         interesting_service_ids,
                                     mediums::AdvertisementReadResult&
                                         advertisement_read_result) {
                                // Th`mutex_` is already held here. Use
                                // `AssumeHeld` tell the thread
                                // annotation static analysis that
                                // `mutex_` is already exclusively
                                // locked.
                                AssumeHeld(mutex_);
                                ProcessFetchGattAdvertisementsRequest(
                                    std::move(peripheral), num_slots, psm,
                                    interesting_service_ids,
                                    advertisement_read_result);
                              });
                    });
                  },
          })) {
    NEARBY_LOGS(INFO) << "Failed to start scan of BLE services.";
    discovered_peripheral_tracker_.StopTracking(service_id);
    // Erase the service id that is just added.
    scanned_service_ids_.erase(service_id);
    return false;
  }

  absl::Duration peripheral_lost_timeout =
      absl::Milliseconds(NearbyFlags::GetInstance().GetInt64Flag(
          config_package_nearby::nearby_connections_feature::
              kBlePeripheralLostTimeoutMillis));
  // Set up lost alarm.
  lost_alarm_ = std::make_unique<CancelableAlarm>(
      "BLE.StartScanning() onLost",
      [this]() {
        MutexLock lock(&mutex_);
        discovered_peripheral_tracker_.ProcessLostGattAdvertisements();
      },
      peripheral_lost_timeout, &alarm_executor_, /*is_recurring=*/true);

  NEARBY_LOGS(INFO) << "Turned on BLE scanning with service id=" << service_id;
  return true;
}

bool BleV2::StopScanning(const std::string& service_id) {
  MutexLock lock(&mutex_);
  if (FeatureFlags::GetInstance().GetFlags().enable_ble_v2_async_scanning) {
    return StopAsyncScanningLocked(service_id);
  }

  if (!IsScanningLocked(service_id)) {
    NEARBY_LOGS(INFO) << "Can't turn off BLE scanning because we never "
                         "started scanning.";
    return false;
  }

  discovered_peripheral_tracker_.StopTracking(service_id);
  NEARBY_LOGS(INFO) << "Turned off BLE scanning with service id=" << service_id;

  scanned_service_ids_.erase(service_id);

  // If still has scanner, don't stop the client scanning.
  if (!scanned_service_ids_.empty()) {
    return true;
  }

  // If no more scanning activities, then stop client scanning.
  NEARBY_LOGS(INFO) << "Turned off BLE client scanning";
  if (lost_alarm_->IsValid()) {
    lost_alarm_->Cancel();
  }
  return medium_.StopScanning();
}

bool BleV2::IsScanning(const std::string& service_id) const {
  MutexLock lock(&mutex_);

  return IsScanningLocked(service_id);
}

bool BleV2::StartAcceptingConnections(const std::string& service_id,
                                      AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);

  if (service_id.empty()) {
    NEARBY_LOGS(INFO)
        << "Refusing to start accepting BLE connections with empty service id.";
    return false;
  }

  if (IsAcceptingConnectionsLocked(service_id)) {
    NEARBY_LOGS(INFO)
        << "Refusing to start accepting BLE connections for " << service_id
        << " because another BLE peripheral socket is already in-progress.";
    return false;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO) << "Can't start accepting BLE connections for "
                      << service_id << " because Bluetooth isn't enabled.";
    return false;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't start accepting BLE connections for "
                      << service_id << " because BLE isn't available.";
    return false;
  }

  BleV2ServerSocket server_socket = medium_.OpenServerSocket(service_id);
  if (!server_socket.IsValid()) {
    NEARBY_LOGS(INFO)
        << "Failed to start accepting Ble connections for service_id="
        << service_id;
    return false;
  }

  // Mark the fact that there's an in-progress Ble server accepting
  // connections.
  auto owned_server_socket =
      server_sockets_.insert({service_id, std::move(server_socket)})
          .first->second;

  // Start the accept loop on a dedicated thread - this stays alive and
  // listening for new incoming connections until StopAcceptingConnections() is
  // invoked.
  accept_loops_runner_.Execute(
      "ble-accept",
      [this, service_id = service_id, callback = std::move(callback),
       server_socket = std::move(owned_server_socket)]() mutable {
        while (true) {
          BleV2Socket client_socket = server_socket.Accept();
          if (!client_socket.IsValid()) {
            NEARBY_LOGS(WARNING) << "The client socket to accept is invalid.";
            server_socket.Close();
            break;
          }
          {
            MutexLock lock(&mutex_);
            client_socket.SetCloseNotifier([this, service_id]() {
              MutexLock lock(&mutex_);
              incoming_sockets_.erase(service_id);
            });
            incoming_sockets_.insert({service_id, client_socket});
          }
          if (callback) {
            callback(std::move(client_socket), service_id);
          }
        }
      });

  return true;
}

bool BleV2::StopAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);

  const auto it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    NEARBY_LOGS(INFO)
        << "Can't stop accepting BLE connections because it was never started.";
    return false;
  }

  // Closing the BleServerSocket will kick off the suicide of the thread
  // in accept_loops_thread_pool_ that blocks on BleServerSocket.accept().
  // That may take some time to complete, but there's no particular reason to
  // wait around for it.
  auto item = server_sockets_.extract(it);

  // Store a handle to the BleServerSocket, so we can use it after
  // removing the entry from server_sockets_; making it scoped
  // is a bonus that takes care of deallocation before we leave this method.
  BleV2ServerSocket& listening_socket = item.mapped();

  // Regardless of whether or not we fail to close the existing
  // BleServerSocket, remove it from server_sockets_ so that it
  // frees up this service for another round.

  // Finally, close the BleServerSocket.
  if (!listening_socket.Close().Ok()) {
    NEARBY_LOGS(INFO) << "Failed to close Ble server socket for service_id="
                      << service_id;
    return false;
  }

  return true;
}

bool BleV2::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsAcceptingConnectionsLocked(service_id);
}

BleV2Socket BleV2::Connect(const std::string& service_id,
                           const BleV2Peripheral& peripheral,
                           CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);
  // Socket to return. To allow for NRVO to work, it has to be a single object.
  BleV2Socket socket;

  if (service_id.empty()) {
    NEARBY_LOGS(INFO) << "Refusing to create client Ble socket because "
                         "service_id is empty.";
    return socket;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't create client Ble socket [service_id="
                      << service_id << "]; Ble isn't available.";
    return socket;
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(INFO) << "Can't create client Ble socket due to cancel.";
    return socket;
  }

  socket = medium_.Connect(service_id,
                           PowerLevelToTxPowerLevel(PowerLevel::kHighPower),
                           peripheral, cancellation_flag);
  if (!socket.IsValid()) {
    NEARBY_LOGS(INFO) << "Failed to Connect via Ble [service_id=" << service_id
                      << "]";
  }

  return socket;
}

bool BleV2::IsAvailableLocked() const { return medium_.IsValid(); }

bool BleV2::IsAdvertisingLocked(const std::string& service_id) const {
  return advertising_infos_.contains(service_id);
}

bool BleV2::IsAdvertisingForLegacyDeviceLocked(
    const std::string& service_id) const {
  return service_ids_to_advertising_sessions_.contains(service_id + "-Legacy");
}

bool BleV2::IsScanningLocked(const std::string& service_id) const {
  if (FeatureFlags::GetInstance().GetFlags().enable_ble_v2_async_scanning) {
    // If using asynchronous scanning, check the corresponding map.
    auto it = service_ids_to_scanning_sessions_.find(service_id);
    return it != service_ids_to_scanning_sessions_.end();
  }
  return scanned_service_ids_.contains(service_id);
}

bool BleV2::IsAcceptingConnectionsLocked(const std::string& service_id) {
  return server_sockets_.contains(service_id);
}

bool BleV2::IsAdvertisementGattServerRunningLocked() {
  return gatt_server_ && gatt_server_->IsValid();
}

bool BleV2::StartAdvertisementGattServerLocked(
    const std::string& service_id, const ByteArray& gatt_advertisement) {
  if (IsAdvertisementGattServerRunningLocked()) {
    NEARBY_LOGS(INFO) << "Advertisement GATT server is not started because one "
                         "is already running.";
    return false;
  }

  std::unique_ptr<GattServer> gatt_server =
      medium_.StartGattServer(/*ServerGattConnectionCallback=*/{});
  if (!gatt_server || !gatt_server->IsValid()) {
    NEARBY_LOGS(INFO) << "Unable to start an advertisement GATT server.";
    return false;
  }

  if (!GenerateAdvertisementCharacteristic(
          /*slot=*/0, gatt_advertisement, *gatt_server)) {
    gatt_server->Stop();
    return false;
  }

  // Insert the advertisements into their open advertisement slots.
  gatt_advertisements_.insert(
      {/*slot=*/0, std::make_pair(service_id, gatt_advertisement)});

  gatt_server_ = std::move(gatt_server);
  return true;
}

bool BleV2::GenerateAdvertisementCharacteristic(
    int slot, const ByteArray& gatt_advertisement, GattServer& gatt_server) {
  GattCharacteristic::Permission permission =
      GattCharacteristic::Permission::kRead;
  GattCharacteristic::Property property = GattCharacteristic::Property::kRead;

  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<Uuid> advertiement_uuid =
      mediums::bleutils::GenerateAdvertisementUuid(slot);
  if (!advertiement_uuid.has_value()) {
    NEARBY_LOGS(INFO) << "Unable to generate advertisement uuid.";
    return false;
  }
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<GattCharacteristic> gatt_characteristic =
      gatt_server.CreateCharacteristic(
          mediums::bleutils::kCopresenceServiceUuid, *advertiement_uuid,
          permission, property);
  if (!gatt_characteristic.has_value()) {
    NEARBY_LOGS(INFO) << "Unable to create and add a characterstic to the gatt "
                         "server for the advertisement.";
    return false;
  }
  if (!gatt_server.UpdateCharacteristic(gatt_characteristic.value(),
                                        gatt_advertisement)) {
    NEARBY_LOGS(INFO) << "Unable to write a value to the GATT characteristic.";
    return false;
  }
  hosted_gatt_characteristics_.insert(gatt_characteristic.value());

  return true;
}

void BleV2::ProcessFetchGattAdvertisementsRequest(
    BleV2Peripheral peripheral, int num_slots, int psm,
    const std::vector<std::string>& interesting_service_ids,
    mediums::AdvertisementReadResult& advertisement_read_result) {
  if (!peripheral.IsValid()) {
    NEARBY_LOGS(INFO) << "Can't read from an advertisement GATT server because "
                         "ble peripheral is null.";
    return;
  }

  if (!radio_.IsEnabled()) {
    NEARBY_LOGS(INFO) << "Can't read from an advertisement GATT server because "
                         "Bluetooth was never turned on.";
    return;
  }

  if (!IsAvailableLocked()) {
    NEARBY_LOGS(INFO) << "Can't read from an advertisement GATT server because "
                         "BLE is not available.";
    return;
  }

  // Connect to a GATT server, reads advertisement data, and then disconnect
  // from the GATT server.
  bool read_success = true;
  std::unique_ptr<GattClient> gatt_client = medium_.ConnectToGattServer(
      std::move(peripheral), PowerLevelToTxPowerLevel(PowerLevel::kHighPower),
      /*ClientGattConnectionCallback=*/{});
  if (!gatt_client || !gatt_client->IsValid()) {
    advertisement_read_result.RecordLastReadStatus(false);
    return;
  }

  // Collect service_uuid and its associated characteristic_uuids.
  absl::flat_hash_map<int, Uuid> slot_characteristic_uuids = {};
  for (int slot = 0; slot < num_slots; ++slot) {
    // Make sure we haven't already read this advertisement before.
    if (advertisement_read_result.HasAdvertisement(slot)) {
      continue;
    }

    // Make sure the characteristic even exists for this slot number. If
    // the characteristic doesn't exist, we shouldn't count the fetch as a
    // failure because there's nothing we could've done about a
    // non-existed characteristic.
    // NOLINTNEXTLINE(google3-legacy-absl-backports)
    absl::optional<Uuid> advertiement_uuid =
        mediums::bleutils::GenerateAdvertisementUuid(slot);
    if (!advertiement_uuid.has_value()) {
      continue;
    }
    slot_characteristic_uuids.insert({slot, *advertiement_uuid});
  }
  if (slot_characteristic_uuids.empty()) {
    // TODO(b/222392304): More test coverage.
    NEARBY_LOGS(WARNING) << "GATT client doesn't have characteristics.";
    advertisement_read_result.RecordLastReadStatus(false);
    return;
  }

  // Discover service and characteristics.
  std::vector<Uuid> characteristic_uuids;
  std::transform(slot_characteristic_uuids.begin(),
                 slot_characteristic_uuids.end(),
                 std::back_inserter(characteristic_uuids),
                 [](auto& kv) { return kv.second; });
  if (!gatt_client->DiscoverServiceAndCharacteristics(
          mediums::bleutils::kCopresenceServiceUuid, characteristic_uuids)) {
    // TODO(b/222392304): More test coverage.
    NEARBY_LOGS(WARNING) << "GATT client doesn't have characteristics.";
    advertisement_read_result.RecordLastReadStatus(false);
    return;
  }

  // Read all advertisements from all characteristics that we haven't read from
  // yet.
  for (const auto& it : slot_characteristic_uuids) {
    int slot = it.first;
    Uuid characteristic_uuid = it.second;
    auto gatt_characteristic = gatt_client->GetCharacteristic(
        mediums::bleutils::kCopresenceServiceUuid, characteristic_uuid);
    if (!gatt_characteristic.has_value()) {
      continue;
    }

    // Read advertisement data from the characteristic associated with this
    // slot.
    auto characteristic_byte =
        gatt_client->ReadCharacteristic(gatt_characteristic.value());
    if (characteristic_byte.has_value()) {
      advertisement_read_result.AddAdvertisement(
          slot, ByteArray(characteristic_byte.value()));
      NEARBY_VLOG(1) << "Successfully read advertisement at slot=" << slot;
    } else {
      NEARBY_LOGS(WARNING) << "Can't read advertisement for slot=" << slot;
      read_success = false;
    }
    // Whether or not we succeeded with this slot, we should try reading the
    // other slots to get as many advertisements as possible before
    // returning a success or failure.
  }
  gatt_client->Disconnect();

  advertisement_read_result.RecordLastReadStatus(read_success);
}

bool BleV2::StopAdvertisementGattServerLocked() {
  if (!IsAdvertisementGattServerRunningLocked()) {
    NEARBY_LOGS(INFO) << "Unable to stop the advertisement GATT server because "
                         "it's not running.";
    return false;
  }

  gatt_server_.reset();
  return true;
}

ByteArray BleV2::CreateAdvertisementHeader(
    int psm, bool extended_advertisement_advertised) {
  // Create a randomized dummy service id to anonymize the header with.
  ByteArray dummy_service_id_bytes =
      Utils::GenerateRandomBytes(kDummyServiceIdLength);
  std::string dummy_service_id{dummy_service_id_bytes};

  mediums::BloomFilter bloom_filter(
      std::make_unique<mediums::BitSetImpl<
          mediums::BleAdvertisementHeader::kServiceIdBloomFilterByteLength>>());
  bloom_filter.Add(dummy_service_id);

  ByteArray advertisement_hash =
      mediums::bleutils::GenerateAdvertisementHash(dummy_service_id_bytes);
  for (const auto& item : gatt_advertisements_) {
    const std::string& service_id = item.second.first;
    const ByteArray& gatt_advertisement = item.second.second;
    bloom_filter.Add(service_id);

    // Compute the next hash.
    std::string advertisement_bodies = absl::StrCat(
        advertisement_hash.AsStringView(), gatt_advertisement.AsStringView());

    advertisement_hash = mediums::bleutils::GenerateAdvertisementHash(
        ByteArray(std::move(advertisement_bodies)));
  }

  return ByteArray(mediums::BleAdvertisementHeader(
      mediums::BleAdvertisementHeader::Version::kV2,
      extended_advertisement_advertised,
      /*num_slots=*/gatt_advertisements_.size(), ByteArray(bloom_filter),
      advertisement_hash, psm));
}

api::ble_v2::BleAdvertisementData
BleV2::CreateAdvertisingDataForLegacyDevice() {
  BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = false;

  ByteArray encoded_bytes{
      mediums::DiscoveredPeripheralTracker::kDummyAdvertisementValue};

  advertising_data.service_data.insert(
      {mediums::bleutils::kCopresenceServiceUuid, encoded_bytes});
  return advertising_data;
}

bool BleV2::StartAdvertisingLocked(const std::string& service_id) {
  const auto it = advertising_infos_.find(service_id);
  if (it == advertising_infos_.end()) {
    NEARBY_LOGS(WARNING) << "Failed to BLE advertise with service_id="
                         << service_id;
    return false;
  }

  const AdvertisingInfo& info = it->second;
  if (info.is_fast_advertisement) {
    return StartFastAdvertisingLocked(service_id, info.power_level,
                                      info.medium_advertisement);
  } else {
    return StartRegularAdvertisingLocked(service_id, info.power_level,
                                         info.medium_advertisement);
  }
}

bool BleV2::StartFastAdvertisingLocked(
    const std::string& service_id, PowerLevel power_level,
    const mediums::BleAdvertisement& medium_advertisement) {
  // Begin building the fast BLE advertisement.
  BleAdvertisementData advertising_data;
  ByteArray medium_advertisement_bytes = ByteArray(medium_advertisement);
  advertising_data.is_extended_advertisement = false;
  advertising_data.service_data.insert(
      {mediums::bleutils::kCopresenceServiceUuid, medium_advertisement_bytes});

  // Finally, start the fast advertising operation.
  if (!medium_.StartAdvertising(
          advertising_data,
          {.tx_power_level = PowerLevelToTxPowerLevel(power_level),
           .is_connectable = true})) {
    NEARBY_LOGS(ERROR) << "Failed to turn on BLE fast advertising with "
                          "advertisement bytes="
                       << absl::BytesToHexString(
                              medium_advertisement_bytes.data());
    return false;
  }

  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnableInstantOnLost)) {
    instant_on_lost_manager_.OnAdvertisingStarted(service_id,
                                                  medium_advertisement_bytes);
  }

  return true;
}

bool BleV2::StartRegularAdvertisingLocked(
    const std::string& service_id, PowerLevel power_level,
    const mediums::BleAdvertisement& medium_advertisement) {
  // Begin building the regular BLE advertisement.
  BleAdvertisementData advertising_data;
  ByteArray medium_advertisement_bytes =
      medium_.IsExtendedAdvertisementsAvailable()
          ? medium_advertisement.ByteArrayWithExtraField()
          : ByteArray(medium_advertisement);

  // Start extended advertisement first if available.
  bool extended_regular_advertisement_success = false;
  if (medium_.IsExtendedAdvertisementsAvailable()) {
    advertising_data.is_extended_advertisement = true;
    advertising_data.service_data.insert(
        {mediums::bleutils::kCopresenceServiceUuid,
         medium_advertisement_bytes});

    // Start the extended regular advertising operation.
    extended_regular_advertisement_success = medium_.StartAdvertising(
        advertising_data,
        {.tx_power_level = PowerLevelToTxPowerLevel(power_level),
         .is_connectable = true});
    if (!extended_regular_advertisement_success) {
      NEARBY_LOGS(ERROR)
          << "Failed to turn on BLE extended regular advertising with "
             "advertisement bytes="
          << absl::BytesToHexString(medium_advertisement_bytes.data());
    } else {
      if (NearbyFlags::GetInstance().GetBoolFlag(
              config_package_nearby::nearby_connections_feature::
                  kEnableInstantOnLost)) {
        instant_on_lost_manager_.OnAdvertisingStarted(
            service_id, medium_advertisement_bytes);
      }
    }
  }

  // Start GATT advertisement no matter extended advertisement succeeded or not.
  // This is to ensure that legacy devices which don't support extended
  // advertisement can get the advertisement via GATT connection.
  bool gatt_advertisement_success = StartGattAdvertisingLocked(
      service_id, power_level, medium_advertisement.GetPsm(),
      medium_advertisement_bytes, extended_regular_advertisement_success);

  return extended_regular_advertisement_success || gatt_advertisement_success;
}

bool BleV2::StartGattAdvertisingLocked(
    const std::string& service_id, PowerLevel power_level, int psm,
    const ByteArray& medium_advertisement_bytes,
    bool extended_advertisement_advertised) {
  // Begin building the GATT BLE advertisement header.
  BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = false;

  // Stop the current advertisement GATT server if there are no incoming
  // sockets connected to this device.
  //
  // The reason for aggressively restarting a GATT server is to make sure this
  // class is not using a stale server object that may not be actually running
  // anymore (possibly due to Bluetooth being turned off).
  //
  // Changing one's GATT server while a remote device is connected to it leads
  // to a loss of GATT callbacks for that remote device. The only time a
  // remote device is indefinitely connected to this device's GATT server is
  // when it has a BLE socket connection.
  if (incoming_sockets_.empty()) {
    NEARBY_VLOG(1)
        << "Aggressively stopping any pre-existing advertisement GATT "
           "servers because no incoming BLE sockets are connected";
    StopAdvertisementGattServerLocked();
  }

  // Start a GATT server to deliver the full advertisement data. If fail to
  // advertise the header, we must shut this down before the method returns.
  if (!IsAdvertisementGattServerRunningLocked()) {
    if (!StartAdvertisementGattServerLocked(service_id,
                                            medium_advertisement_bytes)) {
      NEARBY_LOGS(ERROR)
          << "Failed to turn on BLE GATT advertising for service_id="
          << service_id
          << " because the advertisement GATT server failed to start.";
      return false;
    }
  }

  // Begin building the regular BLE advertisement (backed by a GATT server).
  // Add the advertisement header.
  ByteArray advertisement_header_bytes =
      CreateAdvertisementHeader(psm, extended_advertisement_advertised);
  if (advertisement_header_bytes.Empty()) {
    NEARBY_LOGS(ERROR)
        << "Failed to turn on BLE GATT advertising because we could not "
           "create an advertisement header.";
    // Failed to create an advertisement header, so stop the advertisement
    // GATT server.
    StopAdvertisementGattServerLocked();
    return false;
  }

  advertising_data.service_data.insert(
      {mediums::bleutils::kCopresenceServiceUuid, advertisement_header_bytes});

  // Finally, start the regular advertising operation.
  if (!medium_.StartAdvertising(
          advertising_data,
          {.tx_power_level = PowerLevelToTxPowerLevel(power_level),
           .is_connectable = true})) {
    NEARBY_LOGS(ERROR) << "Failed to turn on BLE GATT advertising with "
                          "advertisement bytes="
                       << absl::BytesToHexString(
                              medium_advertisement_bytes.data());
    // If BLE advertising was not successful, stop the advertisement GATT
    // server.
    StopAdvertisementGattServerLocked();
    return false;
  }

  if (NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_connections_feature::
              kEnableInstantOnLost)) {
    for (const auto& item : advertising_data.service_data) {
      instant_on_lost_manager_.OnAdvertisingStarted(service_id, item.second);
    }
  }

  return true;
}

bool BleV2::StartAsyncScanningLocked(absl::string_view service_id,
                                     PowerLevel power_level) {
  CHECK(FeatureFlags::GetInstance().GetFlags().enable_ble_v2_async_scanning);

  // Use the asynchronous StartScanning method instead of the synchronous one.
  // Note: using FeatureFlags instead of NearbyFlags as there is no Mendel
  // experiment associated with this change, which was driven by ChromeOS.

  // Should we check if the map is not empty, and forego scanning if true?
  // If so, do we store a nullptr instead of a scanning session ptr?

  auto scanning_session = medium_.StartScanning(
      mediums::bleutils::kCopresenceServiceUuid,
      PowerLevelToTxPowerLevel(power_level),
      api::ble_v2::BleMedium::ScanningCallback{
          .start_scanning_result =
              [this, &service_id](absl::Status status) mutable {
                // The `mutex_` is already held here. Use
                // `AssumeHeld` to tell the thread
                // annotation static analysis that
                // `mutex_` is already exclusively
                // locked.
                // Note: unsure if this is always the case, but when I
                // attempted to acquire the lock here I hit a deadlock.
                AssumeHeld(mutex_);
                if (status.ok()) {
                  NEARBY_LOGS(INFO) << "BLE V2 async StartScanning started "
                                       "successfully for service ID"
                                    << &service_id;
                } else {
                  NEARBY_LOGS(ERROR) << "BLE V2 async StartScanning "
                                        "failed for service ID"
                                     << &service_id << ": " << status;
                  service_ids_to_scanning_sessions_.erase(service_id);
                }
              },
          .advertisement_found_cb =
              [this](api::ble_v2::BlePeripheral& peripheral,
                     BleAdvertisementData advertisement_data) {
                AssumeHeld(mutex_);
                BleV2Peripheral proxy(medium_, peripheral);
                RunOnBleThread([this, proxy = std::move(proxy),
                                advertisement_data]() {
                  MutexLock lock(&mutex_);
                  discovered_peripheral_tracker_.ProcessFoundBleAdvertisement(
                      std::move(proxy), advertisement_data,
                      [this](BleV2Peripheral proxy, int num_slots, int psm,
                             const std::vector<std::string>&
                                 interesting_service_ids,
                             mediums::AdvertisementReadResult&
                                 advertisement_read_result) {
                        // The `mutex_` is already held here. Use
                        // `AssumeHeld` to tell the thread
                        // annotation static analysis that
                        // `mutex_` is already exclusively
                        // locked.
                        AssumeHeld(mutex_);
                        ProcessFetchGattAdvertisementsRequest(
                            std::move(proxy), num_slots, psm,
                            interesting_service_ids, advertisement_read_result);
                      });
                });
              },
          .advertisement_lost_cb =
              [](api::ble_v2::BlePeripheral& peripheral) {
                // TODO(b/345514862): Implement.
              },
      });
  service_ids_to_scanning_sessions_.insert(
      {std::string(service_id), std::move(scanning_session)});
  NEARBY_LOGS(INFO) << "Requested to start BLE scanning with service id="
                    << service_id << " size "
                    << service_ids_to_scanning_sessions_.size();

  if (lost_alarm_ != nullptr && lost_alarm_->IsValid()) {
    // We only use one lost alarm, which will check all service IDs for lost
    // advertisements.
    return true;
  }

  absl::Duration peripheral_lost_timeout =
      absl::Milliseconds(NearbyFlags::GetInstance().GetInt64Flag(
          config_package_nearby::nearby_connections_feature::
              kBlePeripheralLostTimeoutMillis));
  // Set up lost alarm.
  lost_alarm_ = std::make_unique<CancelableAlarm>(
      "BLE.StartScanning() onLost",
      [this]() {
        MutexLock lock(&mutex_);
        discovered_peripheral_tracker_.ProcessLostGattAdvertisements();
      },
      peripheral_lost_timeout, &alarm_executor_, /*is_recurring=*/true);
  return true;
}

bool BleV2::StopAsyncScanningLocked(absl::string_view service_id) {
  CHECK(FeatureFlags::GetInstance().GetFlags().enable_ble_v2_async_scanning);
  // If using asynchronous scanning, check the corresponding map.
  auto scanning_session = service_ids_to_scanning_sessions_.find(service_id);
  if (scanning_session == service_ids_to_scanning_sessions_.end()) {
    NEARBY_LOGS(INFO) << "Can't turn off async BLE scanning because we never "
                         "started scanning for this service ID.";
    return false;
  }
  absl::Status status = scanning_session->second->stop_scanning();
  if (!status.ok()) {
    NEARBY_LOGS(WARNING) << "StopAsyncScanningLocked error: " << status;
  }
  service_ids_to_scanning_sessions_.erase(scanning_session);

  NEARBY_LOGS(INFO) << "Turned off BLE client scanning";
  if (lost_alarm_->IsValid()) {
    lost_alarm_->Cancel();
  }
  return true;
}

TxPowerLevel BleV2::PowerLevelToTxPowerLevel(PowerLevel power_level) {
  switch (power_level) {
    case PowerLevel::kHighPower:
      return TxPowerLevel::kHigh;
    case PowerLevel::kLowPower:
      // Medium power is about the size of a conference room.
      // Any lower and we won't be visible at a distance.
      return TxPowerLevel::kMedium;
    default:
      return TxPowerLevel::kUnknown;
  }
}

void BleV2::RunOnBleThread(Runnable runnable) {
  serial_executor_.Execute(std::move(runnable));
}

}  // namespace connections
}  // namespace nearby
