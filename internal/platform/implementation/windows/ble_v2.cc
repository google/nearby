// Copyright 2022-2023 Google LLC
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

#include "internal/platform/implementation/windows/ble_v2.h"

#include <algorithm>
#include <chrono>  // NOLINT
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"
#include "connections/implementation/mediums/ble_v2/bloom_filter.h"
#include "connections/implementation/mediums/utils.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/ble_gatt_client.h"
#include "internal/platform/implementation/windows/ble_gatt_server.h"
#include "internal/platform/implementation/windows/ble_v2_peripheral.h"
#include "internal/platform/implementation/windows/ble_v2_server_socket.h"
#include "internal/platform/implementation/windows/ble_v2_socket.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/prng.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Storage.Streams.h"  // NOLINT(misc-include-cleaner)

namespace nearby {
namespace windows {

namespace {

using ::absl::Milliseconds;
using ::absl::Minutes;
using ::nearby::api::ble_v2::AdvertiseParameters;
using ::nearby::api::ble_v2::BleAdvertisementData;
using ::nearby::api::ble_v2::BleServerSocket;
using ::nearby::api::ble_v2::BleSocket;
using ::nearby::api::ble_v2::GattClient;
using ::nearby::api::ble_v2::ServerGattConnectionCallback;
using ::nearby::api::ble_v2::TxPowerLevel;
using ::nearby::connections::mediums::BitSetImpl;
using ::nearby::connections::mediums::BleAdvertisementHeader;
using ::nearby::connections::mediums::BloomFilter;
using ::winrt::Windows::Devices::Bluetooth::BluetoothError;
using ::winrt::Windows::Devices::Bluetooth::BluetoothLEDevice;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisement;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataSection;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataTypes;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementFilter;  // NOLINT(misc-include-cleaner)
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisher;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatusChangedEventArgs;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementReceivedEventArgs;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcher;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStatus;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStoppedEventArgs;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEScanningMode;
using ::winrt::Windows::Foundation::TimeSpan;
using ::winrt::Windows::Foundation::TypedEventHandler;
using ::winrt::Windows::Storage::Streams::Buffer;
using ::winrt::Windows::Storage::Streams::ByteOrder;
using ::winrt::Windows::Storage::Streams::DataReader;
using ::winrt::Windows::Storage::Streams::DataWriter;

template <typename T>
using IVector = winrt::Windows::Foundation::Collections::IVector<T>;

std::string TxPowerLevelToName(TxPowerLevel tx_power_level) {
  switch (tx_power_level) {
    case TxPowerLevel::kUltraLow:
      return "UltraLow";
    case TxPowerLevel::kLow:
      return "Low";
    case TxPowerLevel::kMedium:
      return "Medium";
    case TxPowerLevel::kHigh:
      return "High";
    case TxPowerLevel::kUnknown:
      return "Unknown";
  }
}

// Creates a fake AdvertisementHeader for when an alternate service UUID is
// discovered, to trigger retrieval of GATT characteristics.
// Use the device MAC address as the advertisement hash so that we have a unique
// advertisement header for each device.
BleAdvertisementHeader CreateAdvertisementHeader(
    const std::string& mac_address,
    const std::vector<std::string>& service_ids) {
  BloomFilter bloom_filter(
      std::make_unique<BitSetImpl<
          BleAdvertisementHeader::kServiceIdBloomFilterByteLength>>());
  for (const auto& service_id : service_ids) {
    bloom_filter.Add(service_id);
  }
  return BleAdvertisementHeader(
      BleAdvertisementHeader::Version::kV2,
      /*support_extended_advertisement=*/false,
      /*num_slots=*/1, ByteArray(bloom_filter),
      /*advertisement_hash=*/
      connections::Utils::Sha256Hash(
          mac_address, BleAdvertisementHeader::kAdvertisementHashByteLength),
      /*psm=*/BleAdvertisementHeader::kDefaultPsmValue);
}

// Max times trying to generate unused session id.
static constexpr uint64_t kGenerateSessionIdRetryLimit = 3;
// Indicating failed to generate unused session id.
static constexpr uint64_t kFailedGenerateSessionId = 0;

constexpr absl::Duration kMediumTimeout = Milliseconds(500);
constexpr absl::Duration kMediumCheckInterval = Milliseconds(50);

// Remove lost/unused peripherals after a timeout.
constexpr absl::Duration kPeripheralExpiryTime = Minutes(15);
// Prevent too frequent cleanup tasks.
constexpr absl::Duration kMaxPeripheralCleanupFrequency = Minutes(3);

// Service Data - 16-bit UUID.  From Bluetooth Assigned Numbers Section 2.3.
constexpr uint8_t kUuid16ServiceDataType = 0x16;
}  // namespace

BleV2Medium::AdvertisementWatcher::~AdvertisementWatcher() { Stop(); }

void BleV2Medium::AdvertisementWatcher::Initialize(
    const TypedEventHandler<BluetoothLEAdvertisementWatcher,
                            BluetoothLEAdvertisementReceivedEventArgs>&
        received_handler,
    const TypedEventHandler<BluetoothLEAdvertisementWatcher,
                            BluetoothLEAdvertisementWatcherStoppedEventArgs>&
        stopped_handler) {
  absl::MutexLock lock(&mutex_);
  try {
    watcher_token_ = watcher_.Stopped(stopped_handler);
    advertisement_received_token_ = watcher_.Received(received_handler);
    watcher_.AllowExtendedAdvertisements(true);

    // Active mode indicates that scan request packets will be sent to query
    // for Scan Response
    watcher_.ScanningMode(BluetoothLEScanningMode::Active);
    ::winrt::Windows::Devices::Bluetooth::BluetoothSignalStrengthFilter filter;
    filter.SamplingInterval(TimeSpan(std::chrono::seconds(2)));
    watcher_.SignalStrengthFilter(filter);
    initialized_ = true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Exception on BLE scan initialize: " << exception.what();
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__
               << ": Exception on BLE scan initialize: " << ex.code() << ": "
               << winrt::to_string(ex.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception on BLE scan initialize.";
  }
}

void BleV2Medium::AdvertisementWatcher::InitializeWithServiceFilter(
    uint16_t service_uuid,
    const TypedEventHandler<BluetoothLEAdvertisementWatcher,
                            BluetoothLEAdvertisementReceivedEventArgs>&
        received_handler,
    const TypedEventHandler<BluetoothLEAdvertisementWatcher,
                            BluetoothLEAdvertisementWatcherStoppedEventArgs>&
        stopped_handler) {
  absl::MutexLock lock(&mutex_);
  try {
    watcher_token_ = watcher_.Stopped(stopped_handler);
    advertisement_received_token_ = watcher_.Received(received_handler);
    watcher_.AllowExtendedAdvertisements(true);

    // Active mode indicates that scan request packets will be sent to query
    // for Scan Response
    watcher_.ScanningMode(BluetoothLEScanningMode::Active);
    BluetoothLEAdvertisementDataSection data_section;
    data_section.DataType(kUuid16ServiceDataType);
    DataWriter data_writer;  // NOLINT(misc-include-cleaner)
    data_writer.ByteOrder(ByteOrder::LittleEndian);
    data_writer.WriteInt16(service_uuid);
    data_section.Data(data_writer.DetachBuffer());
    BluetoothLEAdvertisement advertisement;
    advertisement.DataSections().Append(data_section);
    BluetoothLEAdvertisementFilter advertisement_filter;
    advertisement_filter.Advertisement(advertisement);
    watcher_.AdvertisementFilter(advertisement_filter);
    initialized_ = true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Exception on BLE scan initialize: " << exception.what();
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__
               << ": Exception on BLE scan initialize, hresult: " << ex.code()
               << ": " << winrt::to_string(ex.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception on BLE scan initialize.";
  }
}

bool BleV2Medium::AdvertisementWatcher::Start() {
  absl::MutexLock lock(&mutex_);
  if (!initialized_) {
    LOG(ERROR) << __func__ << ": Failed to start BLE watcher: not initialized.";
    return false;
  }
  try {
    watcher_.Start();
    // Wait for watcher to transition to Started state.
    // Any state other than Started or Created is considered a failure.
    absl::Duration wait_duration;
    while (wait_duration < kMediumTimeout &&
           watcher_.Status() ==
               BluetoothLEAdvertisementWatcherStatus::Created) {
      absl::SleepFor(kMediumCheckInterval);
      wait_duration += kMediumCheckInterval;
    }
    BluetoothLEAdvertisementWatcherStatus status = watcher_.Status();
    if (status != BluetoothLEAdvertisementWatcherStatus::Started) {
      LOG(ERROR) << __func__ << ": Failed to start BLE watcher: "
                 << static_cast<int>(status);
      return false;
    }
    LOG(INFO) << __func__ << ": BLE scanning started.";
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Exception on BLE scan start: " << exception.what();
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__
               << ": Exception on  BLE scan start, hresult: " << ex.code()
               << ": " << winrt::to_string(ex.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception on BLE scan start.";
  }
  return true;
}

void BleV2Medium::AdvertisementWatcher::Stop() {
  absl::MutexLock lock(&mutex_);
  try {
    watcher_.Stop();
    if (watcher_token_) {
      watcher_.Stopped(watcher_token_);
    }
    if (advertisement_received_token_) {
      watcher_.Received(advertisement_received_token_);
    }
    initialized_ = false;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Exception on BLE scan stop: " << exception.what();
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__
               << ": Exception on BLE scan stop, hresult: " << ex.code() << ": "
               << winrt::to_string(ex.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception on BLE scan stop.";
  }
}

BleV2Medium::BleV2Medium(api::BluetoothAdapter& adapter)
    : adapter_(dynamic_cast<BluetoothAdapter*>(&adapter)) {}

// Advertisement packet and populate accordingly.
bool BleV2Medium::StartAdvertising(const BleAdvertisementData& advertising_data,
                                   AdvertiseParameters advertising_parameters) {
  absl::MutexLock lock(&mutex_);
  std::string service_data_info;
  for (const auto& it : advertising_data.service_data) {
    service_data_info += "{uuid:" + std::string(it.first) +
                         ",data size:" + absl::StrCat(it.second.size()) +
                         ", data=0x" +
                         absl::BytesToHexString(it.second.AsStringView()) + "}";
  }

  LOG(INFO) << __func__
            << ": advertising_data.service_data=" << service_data_info
            << ", tx_power_level="
            << TxPowerLevelToName(advertising_parameters.tx_power_level);

  if (advertising_data.is_extended_advertisement) {
    // In BLE v2, the flag is set when the Bluetooth adapter supports extended
    // advertising and GATT server is using.
    LOG(INFO) << __func__ << ": BLE advertising using BLE extended feature.";
    return StartBleAdvertising(advertising_data, advertising_parameters);
  } else {
    if (ble_gatt_server_ != nullptr) {
      LOG(INFO) << __func__ << ": BLE advertising on GATT server.";
      return StartGattAdvertising(advertising_data, advertising_parameters);
    } else {
      LOG(INFO) << __func__ << ": BLE fast advertising.";
      return StartBleAdvertising(advertising_data, advertising_parameters);
    }
  }
}

bool BleV2Medium::StopAdvertising() {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << __func__ << ": Stop advertising.";
  bool result = true;
  if (is_gatt_publisher_started_) {
    bool stop_gatt_result = StopGattAdvertising();
    if (!stop_gatt_result) {
      LOG(WARNING) << "Failed to stop GATT advertising.";
    }
    ble_gatt_server_ = nullptr;
    result = stop_gatt_result;
  }

  if (is_ble_publisher_started_) {
    bool stop_ble_result = StopBleAdvertising();
    if (!stop_ble_result) {
      LOG(WARNING) << "Failed to stop BLE advertising.";
    }
    result = result && stop_ble_result;
  }

  return result;
}

// TODO(hais) manually verify the new api before switching async flags.
std::unique_ptr<BleV2Medium::AdvertisingSession> BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_set_parameters,
    BleV2Medium::AdvertisingCallback callback) {
  LOG(INFO) << __func__ << ": advertising_data.is_extended_advertisement="
            << advertising_data.is_extended_advertisement
            << ", advertising_data.service_data size="
            << advertising_data.service_data.size() << ", tx_power_level="
            << TxPowerLevelToName(advertise_set_parameters.tx_power_level)
            << ", is_connectable=" << advertise_set_parameters.is_connectable;
  if (StartAdvertising(advertising_data, advertise_set_parameters)) {
    if (callback.start_advertising_result) {
      callback.start_advertising_result(absl::OkStatus());
    }
  } else {
    if (callback.start_advertising_result) {
      callback.start_advertising_result(
          absl::InternalError("Failed to start advertising."));
    }
    return nullptr;
  }

  return std::make_unique<BleV2Medium::AdvertisingSession>(
      BleV2Medium::AdvertisingSession{
          .stop_advertising =
              [this]() {
                if (StopAdvertising()) {
                  return absl::OkStatus();
                } else {
                  return absl::InternalError("Failed to stop advertising.");
                }
              },
      });
}

std::unique_ptr<BleV2Medium::AdvertisementWatcher>
BleV2Medium::CreateBleWatcher(uint16_t service_uuid) {
  auto watcher = std::make_unique<AdvertisementWatcher>();
  watcher->InitializeWithServiceFilter(
      service_uuid, {this, &BleV2Medium::AdvertisementReceivedHandler},
      {this, &BleV2Medium::WatcherHandler});

  if (!watcher->Start()) {
    return nullptr;
  }
  LOG(INFO) << __func__ << ": BLE scanning started for service uuid "
            << absl::StrCat(absl::Hex(service_uuid));
  return watcher;
}

bool BleV2Medium::StartScanning(const Uuid& service_uuid,
                                TxPowerLevel tx_power_level,
                                ScanCallback callback) {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << __func__ << ": service UUID: " << std::string(service_uuid)
            << ", TxPowerLevel: " << TxPowerLevelToName(tx_power_level);
  if (!adapter_->IsEnabled()) {
    LOG(WARNING) << __func__
                 << "BLE cannot start scanning because the "
                    "Bluetooth adapter is not enabled.";
    return false;
  }

  if (!watchers_.empty()) {
    LOG(WARNING) << __func__
                 << ": BLE cannot start to scan again when it is running.";
    return false;
  }

  service_uuid_ = service_uuid;
  tx_power_level_ = tx_power_level;
  scan_callback_ = std::move(callback);

  // This assumes that service_uuid has a valid UUID16 alias.
  // Verify that service_uuid in in valid range.
  std::optional<uint16_t> service_uuid16 = service_uuid_.GetBtUuid16();
  if (!service_uuid16.has_value()) {
    LOG(WARNING) << __func__
                 << ": BLE cannot start to scan because the service UUID is "
                    "not in valid range.";
    return false;
  }

  service_uuid16_ = *service_uuid16;
  std::unique_ptr<AdvertisementWatcher> watcher =
      CreateBleWatcher(service_uuid16_);
  if (watcher == nullptr) {
    return false;
  }

  // Watch for alternate service UUIDs only if extended advertising is not
  // supported.
  if (!adapter_->IsExtendedAdvertisingSupported()) {
    watchers_.reserve(alternate_uuids_for_service_.size() + 1);
    for (const auto& [alternate_uuid16, service_id] :
         alternate_uuids_for_service_) {
      std::unique_ptr<AdvertisementWatcher> alternate_watcher =
          CreateBleWatcher(alternate_uuid16);
      if (alternate_watcher == nullptr) {
        watchers_.clear();
        return false;
      }
      watchers_.push_back(std::move(alternate_watcher));
    }
  }
  watchers_.push_back(std::move(watcher));
  LOG(INFO) << __func__ << ": BLE scanning started.";
  return true;
}

std::unique_ptr<BleV2Medium::ScanningSession> BleV2Medium::StartScanning(
    const Uuid& service_uuid, TxPowerLevel tx_power_level,
    BleV2Medium::ScanningCallback callback) {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << __func__ << ": service UUID: " << std::string(service_uuid)
            << ", TxPowerLevel: " << TxPowerLevelToName(tx_power_level);
  if (!adapter_->IsEnabled()) {
    LOG(WARNING) << __func__
                 << "BLE cannot start scanning because the "
                    "Bluetooth adapter is not enabled.";
    return nullptr;
  }
  if (watchers_.empty()) {
    LOG(WARNING) << __func__ << ": Starting BLE Scanning.";
    auto watcher = std::make_unique<AdvertisementWatcher>();
    watcher->Initialize({this, &BleV2Medium::AdvertisementFoundHandler},
                        {this, &BleV2Medium::WatcherHandler});
    if (!watcher->Start()) {
      return nullptr;
    }
    watchers_.push_back(std::move(watcher));
    LOG(INFO) << __func__ << ": BLE scanning started.";
  } else {
    LOG(WARNING) << __func__ << ": BLE Scanning already started.";
  }
  uint64_t session_id = GenerateSessionId();

  auto iter = service_uuid_to_session_map_.find(service_uuid);
  if (iter == service_uuid_to_session_map_.end()) {
    service_uuid_to_session_map_[service_uuid].insert(
        {session_id, std::move(callback)});
  } else {
    iter->second.insert({session_id, std::move(callback)});
  }

  // Generate and return ScanningSession.
  return std::make_unique<BleV2Medium::ScanningSession>(
      BleV2Medium::ScanningSession{
          .stop_scanning =
              [this, session_id, service_uuid]() {
                size_t num_erased_from_service_and_session_map = 0u;
                {
                  absl::MutexLock lock(&mutex_);
                  auto iter = service_uuid_to_session_map_.find(service_uuid);
                  if (iter != service_uuid_to_session_map_.end()) {
                    num_erased_from_service_and_session_map =
                        iter->second.erase(session_id);
                  }
                  if (num_erased_from_service_and_session_map != 1u) {
                    return absl::NotFoundError(
                        "Can't find the provided internal session");
                  }

                  if (service_uuid_to_session_map_[service_uuid].empty()) {
                    service_uuid_to_session_map_.erase(service_uuid);
                  }
                  // Stop discovery if there's no more on-going scan sessions.
                  if (service_uuid_to_session_map_.empty()) {
                    try {
                      LOG(INFO)
                          << "No more scan sessions, stopping Ble scanning.";
                      watchers_.clear();

                      LOG(INFO) << "Ble stoped scanning successfully.";
                    } catch (std::exception exception) {
                      LOG(ERROR)
                          << __func__ << ": Exception to stop BLE scanning: "
                          << exception.what();
                      return absl::InternalError(
                          "Bad status stopping Ble scanning");
                    } catch (const winrt::hresult_error& ex) {
                      LOG(ERROR)
                          << __func__
                          << ": Exception to stop BLE scanning: " << ex.code()
                          << ": " << winrt::to_string(ex.message());
                      return absl::InternalError(
                          "Bad status stopping Ble scanning");
                    } catch (...) {
                      LOG(ERROR) << __func__ << ": Unknown exception.";
                      return absl::InternalError(
                          "Bad status stopping Ble scanning");
                    }
                  }
                }
                return absl::OkStatus();
              },
      });
}

std::unique_ptr<api::ble_v2::GattServer> BleV2Medium::StartGattServer(
    api::ble_v2::ServerGattConnectionCallback callback) {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << __func__ << ": Start GATT server.";

  if (!NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableBleV2Gatt)) {
    if (adapter_->IsExtendedAdvertisingSupported()) {
      LOG(WARNING) << __func__ << ": GATT is disabled.";
      return nullptr;
    }

    if (!NearbyFlags::GetInstance().GetBoolFlag(
            platform::config_package_nearby::nearby_platform_feature::
                kEnableBleV2GattOnNonExtendedDevice)) {
      LOG(WARNING) << __func__ << ": GATT is disabled.";
      return nullptr;
    }
  }
  auto gatt_server =
      std::make_unique<BleGattServer>(adapter_, std::move(callback));

  ble_gatt_server_ = gatt_server.get();
  ble_gatt_server_->SetCloseNotifier([this]() {
    // In avoid to create a new thread to close the gatt server, we don't
    // acquire the mutex here. The calling flow may cause deadlock due to
    // StartGattAdvertising may run into the codes. It is not ideal, but it is
    // hard to run in thread issues.
    LOG(INFO) << __func__ << ": GATT server is closed.";
    ble_gatt_server_ = nullptr;
  });

  return gatt_server;
}

std::unique_ptr<api::ble_v2::GattClient> BleV2Medium::ConnectToGattServer(
    api::ble_v2::BlePeripheral& peripheral, TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << "ConnectToGattServer is called, address: "
            << peripheral.GetAddress()
            << ", power:" << TxPowerLevelToName(tx_power_level);

  if (!NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableBleV2Gatt)) {
    if (adapter_->IsExtendedAdvertisingSupported()) {
      LOG(WARNING) << __func__ << ": GATT is disabled.";
      return nullptr;
    }

    if (!NearbyFlags::GetInstance().GetBoolFlag(
            platform::config_package_nearby::nearby_platform_feature::
                kEnableBleV2GattOnNonExtendedDevice)) {
      LOG(WARNING) << __func__ << ": GATT is disabled.";
      return nullptr;
    }
  }

  try {
    BluetoothLEDevice ble_device =
        BluetoothLEDevice::FromBluetoothAddressAsync(
            mac_address_string_to_uint64(peripheral.GetAddress()))
            .get();

    return std::make_unique<BleGattClient>(ble_device);
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": WinRT exception: " << error.code() << ": "
               << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
  }

  return nullptr;
}

bool BleV2Medium::StopScanning() {
  absl::MutexLock lock(&mutex_);

  alternate_uuids_for_service_.clear();

  LOG(INFO) << __func__ << ": BLE StopScanning: service_uuid: "
            << absl::StrCat(absl::Hex(service_uuid16_));
  if (!adapter_->IsEnabled()) {
    LOG(WARNING) << "BLE cannot stop scanning because the "
                    "bluetooth adapter is not enabled.";
    return false;
  }

  if (watchers_.empty()) {
    LOG(WARNING) << "BLE scanning is not running.";
    return false;
  }

  watchers_.clear();

  LOG(ERROR) << "Windows Ble stoped scanning successfully for service UUID: "
             << absl::StrCat(absl::Hex(service_uuid16_));
  return true;
}

std::unique_ptr<api::ble_v2::BleServerSocket> BleV2Medium::OpenServerSocket(
    const std::string& service_id) {
  LOG(INFO) << "OpenServerSocket is called";

  auto server_socket = std::make_unique<BleV2ServerSocket>(adapter_);

  if (!server_socket->Bind()) {
    LOG(ERROR) << __func__ << ": Failed to bing socket.";
    return nullptr;
  }

  return server_socket;
}

std::unique_ptr<api::ble_v2::BleSocket> BleV2Medium::Connect(
    const std::string& service_id, TxPowerLevel tx_power_level,
    api::ble_v2::BlePeripheral& remote_peripheral,
    CancellationFlag* cancellation_flag) {
  LOG(INFO) << __func__ << ": Connect to service_id=" << service_id;

  if (cancellation_flag == nullptr) {
    LOG(ERROR) << __func__ << ": cancellation_flag not specified.";
    return nullptr;
  }

  if (cancellation_flag->Cancelled()) {
    LOG(INFO) << __func__ << ": BLE socket connection cancelled for service: "
              << service_id;
    return nullptr;
  }

  auto ble_socket = std::make_unique<BleV2Socket>();

  nearby::CancellationFlagListener cancellation_flag_listener(
      cancellation_flag, [socket = ble_socket.get()]() { socket->Close(); });

  if (!ble_socket->Connect(&remote_peripheral)) {
    LOG(INFO) << __func__
              << ": BLE socket connection failed. service_id=" << service_id;
    return nullptr;
  }

  return ble_socket;
}

bool BleV2Medium::IsExtendedAdvertisementsAvailable() {
  return adapter_->IsExtendedAdvertisingSupported();
}

bool BleV2Medium::StartBleAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertising_parameters) {
  LOG(INFO) << __func__ << ": Start BLE advertising.";
  try {
    if (!adapter_->IsEnabled()) {
      LOG(WARNING) << "BLE cannot start advertising because the "
                      "bluetooth adapter is not enabled.";
      return false;
    }

    if (advertising_data.service_data.empty()) {
      LOG(WARNING)
          << "BLE cannot start to advertise due to invalid service data.";
      return false;
    }

    if (is_ble_publisher_started_) {
      LOG(WARNING) << "BLE cannot start to advertise again when it is running.";
      return false;
    }

    BluetoothLEAdvertisement advertisement;
    IVector<BluetoothLEAdvertisementDataSection> data_sections =
        advertisement.DataSections();

    int max_data_section_size = 0;

    for (const auto& it : advertising_data.service_data) {
      DataWriter data_writer;
      data_writer.ByteOrder(ByteOrder::LittleEndian);

      std::optional<uint16_t> uuid16 = it.first.GetBtUuid16();
      if (!uuid16.has_value()) {
        LOG(WARNING) << "BLE failed to get service UUID.";
        return false;
      }
      LOG(WARNING) << "BLE service UUID: " << absl::StrCat(absl::Hex(*uuid16));

      data_writer.WriteUInt16(*uuid16);

      for (int i = 0; i < it.second.size(); ++i) {
        data_writer.WriteByte(static_cast<uint8_t>(*(it.second.data() + i)));
      }

      if (max_data_section_size < it.second.size()) {
        max_data_section_size = it.second.size();
      }

      BluetoothLEAdvertisementDataSection ble_service_data =
          BluetoothLEAdvertisementDataSection(kUuid16ServiceDataType,
                                              data_writer.DetachBuffer());

      data_sections.Append(ble_service_data);
    }

    advertisement.DataSections() = data_sections;

    // Use Extended Advertising if Fast Advertisement Service Uuid is empty
    // string because the long format advertisement will be used
    if (advertising_data.is_extended_advertisement) {
      if (!adapter_->IsExtendedAdvertisingSupported()) {
        LOG(WARNING)
            << "Cannot advertise extended advertisement on devie without BLE "
               "advertisement extention feature.";
        return false;
      }

      publisher_ = BluetoothLEAdvertisementPublisher(advertisement);
      publisher_.UseExtendedAdvertisement(true);
    } else {
      if (max_data_section_size > 27) {
        LOG(WARNING) << "Invalid advertisement data size for "
                        "non-extended advertisement.";
        return false;
      }

      publisher_ = BluetoothLEAdvertisementPublisher(advertisement);
      publisher_.UseExtendedAdvertisement(false);
    }
    publisher_token_ =
        publisher_.StatusChanged({this, &BleV2Medium::PublisherHandler});

    publisher_.Start();

    // Wait for the publisher to start.
    absl::Duration wait_duration;
    while (publisher_.Status() !=
           BluetoothLEAdvertisementPublisherStatus::Started) {
      absl::SleepFor(kMediumCheckInterval);
      wait_duration += kMediumCheckInterval;
      if (wait_duration > kMediumTimeout) {
        LOG(ERROR) << __func__
                   << ": BLE advertising failed to start due to timeout.";
        publisher_.StatusChanged(publisher_token_);
        publisher_ = nullptr;
        is_ble_publisher_started_ = false;
        return false;
      }
    }

    is_ble_publisher_started_ = true;
    LOG(INFO) << "BLE advertising started.";
    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Exception to start BLE advertising: " << exception.what();

    return false;
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__
               << ": Exception to start BLE advertising: " << ex.code() << ": "
               << winrt::to_string(ex.message());

    return false;
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
    return false;
  }
}

bool BleV2Medium::StopBleAdvertising() {
  LOG(INFO) << __func__ << ": Stop BLE advertising.";
  try {
    if (!adapter_->IsEnabled()) {
      LOG(WARNING) << "BLE cannot stop advertising because the "
                      "bluetooth adapter is not enabled.";
      return false;
    }

    if (!is_ble_publisher_started_) {
      LOG(WARNING) << "BLE advertising is not running.";
      return false;
    }

    // publisher_ may be null when status changed during advertising.
    if (publisher_ == nullptr ||
        publisher_.Status() !=
            BluetoothLEAdvertisementPublisherStatus::Started) {
      LOG(WARNING) << "No started publisher is running.";
      return false;
    }

    publisher_.Stop();

    // Wait for the publisher to stop.
    absl::Duration wait_duration;
    while (publisher_.Status() !=
           BluetoothLEAdvertisementPublisherStatus::Stopped) {
      absl::SleepFor(kMediumCheckInterval);
      wait_duration += kMediumCheckInterval;
      if (wait_duration > kMediumTimeout) {
        LOG(ERROR) << __func__
                   << ": BLE advertising failed to stop due to timeout.";
        publisher_.StatusChanged(publisher_token_);
        publisher_ = nullptr;
        is_ble_publisher_started_ = false;
        return false;
      }
    }

    // Reset publisher.
    publisher_.StatusChanged(publisher_token_);
    publisher_ = nullptr;
    is_ble_publisher_started_ = false;

    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Exception to stop BLE advertising: " << exception.what();

    return false;
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__
               << ": Exception to stop BLE advertising: " << ex.code() << ": "
               << winrt::to_string(ex.message());

    return false;
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
    return false;
  }
}

bool BleV2Medium::StartGattAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertising_parameters) {
  LOG(INFO) << __func__ << ": Start GATT advertising.";
  try {
    if (!adapter_->IsEnabled()) {
      LOG(WARNING) << "BLE cannot start GATT advertising because the "
                      "bluetooth adapter is not enabled.";
      return false;
    }

    if (advertising_data.service_data.empty()) {
      LOG(WARNING)
          << "BLE cannot start GATT advertising due to invalid service data.";
      return false;
    }

    if (is_gatt_publisher_started_) {
      LOG(WARNING)
          << "BLE cannot start GATT advertising again when it is running.";
      return false;
    }

    if (ble_gatt_server_ == nullptr) {
      LOG(WARNING) << "No Gatt server is running.";
      return false;
    }

    // This is GATT server advertisement, find service data first.
    ByteArray service_data;

    for (const auto& it : advertising_data.service_data) {
      service_data = it.second;
      break;
    }

    bool is_started = ble_gatt_server_->StartAdvertisement(
        service_data, advertising_parameters.is_connectable);
    if (!is_started) {
      LOG(WARNING) << "BLE cannot start GATT advertising.";
      return false;
    }

    is_gatt_publisher_started_ = true;

    LOG(INFO) << "GATT advertising started.";
    return true;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__
               << ": Exception to start GATT advertising: " << exception.what();

    return false;
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__
               << ": Exception to start GATT advertising: " << ex.code() << ": "
               << winrt::to_string(ex.message());

    return false;
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
    return false;
  }
}

bool BleV2Medium::StopGattAdvertising() {
  try {
    LOG(INFO) << __func__ << ": Stop GATT advertising.";
    if (!adapter_->IsEnabled()) {
      LOG(WARNING) << "BLE cannot stop GATT advertising because the "
                      "bluetooth adapter is not enabled.";
      return false;
    }

    if (!is_gatt_publisher_started_) {
      LOG(WARNING) << "BLE GATT advertising is not running.";
      return false;
    }

    if (ble_gatt_server_ == nullptr) {
      LOG(WARNING) << "No Gatt server is running.";
      return false;
    }

    bool stop_result = ble_gatt_server_->StopAdvertisement();
    is_gatt_publisher_started_ = false;

    LOG(INFO) << "Stop GATT advertisement result=" << stop_result;
    return stop_result;
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Exception to stop BLE GATT advertising: "
               << exception.what();

    return false;
  } catch (const winrt::hresult_error& ex) {
    LOG(ERROR) << __func__
               << ": Exception to stop BLE GATT advertising: " << ex.code()
               << ": " << winrt::to_string(ex.message());

    return false;
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exception.";
    return false;
  }
}

void BleV2Medium::PublisherHandler(
    BluetoothLEAdvertisementPublisher publisher,
    BluetoothLEAdvertisementPublisherStatusChangedEventArgs args) {
  // This method is called when publisher's status is changed.
  switch (args.Status()) {
    case BluetoothLEAdvertisementPublisherStatus::Created:
      LOG(INFO) << "Nearby BLE Medium created to advertise.";
      return;
    case BluetoothLEAdvertisementPublisherStatus::Started:
      LOG(INFO) << "Nearby BLE Medium started to advertise.";
      return;
    case BluetoothLEAdvertisementPublisherStatus::Stopping:
      LOG(INFO) << "Nearby BLE Medium is stopping.";
      return;
    case BluetoothLEAdvertisementPublisherStatus::Waiting:
      LOG(INFO) << "Nearby BLE Medium is waiting.";
      return;
    case BluetoothLEAdvertisementPublisherStatus::Stopped:
      LOG(INFO) << "Nearby BLE Medium stopped to advertise.";
      break;
    case BluetoothLEAdvertisementPublisherStatus::Aborted:
      switch (args.Error()) {
        case BluetoothError::Success:
          if (publisher.Status() ==
              BluetoothLEAdvertisementPublisherStatus::Started) {
            LOG(ERROR) << "Nearby BLE Medium start advertising operation was "
                          "successfully completed or serviced.";
          }
          if (publisher.Status() ==
              BluetoothLEAdvertisementPublisherStatus::Stopped) {
            LOG(ERROR) << "Nearby BLE Medium stop advertising operation was "
                          "successfully completed or serviced.";
          } else {
            LOG(ERROR) << "Nearby BLE Medium advertising failed due to "
                          "unknown errors.";
          }
          break;
        case BluetoothError::RadioNotAvailable:
          LOG(ERROR) << "Nearby BLE Medium advertising failed due to "
                        "radio not available.";
          break;
        case BluetoothError::ResourceInUse:
          LOG(ERROR)
              << "Nearby BLE Medium advertising failed due to resource in use.";
          break;
        case BluetoothError::DeviceNotConnected:
          LOG(ERROR) << "Nearby BLE Medium advertising failed due to "
                        "remote device is not connected.";
          break;
        case BluetoothError::DisabledByPolicy:
          LOG(ERROR) << "Nearby BLE Medium advertising failed due to "
                        "disabled by policy.";
          break;
        case BluetoothError::DisabledByUser:
          LOG(ERROR) << "Nearby BLE Medium advertising failed due to "
                        "disabled by user.";
          break;
        case BluetoothError::NotSupported:
          LOG(ERROR) << "Nearby BLE Medium advertising failed due to "
                        "hardware not supported.";
          break;
        case BluetoothError::TransportNotSupported:
          LOG(ERROR) << "Nearby BLE Medium advertising failed due to "
                        "transport not supported.";
          break;
        case BluetoothError::ConsentRequired:
          LOG(ERROR) << "Nearby BLE Medium advertising failed due to "
                        "consent required.";
          break;
        case BluetoothError::OtherError:
        default:
          LOG(ERROR)
              << "Nearby BLE Medium advertising failed due to unknown errors.";
          break;
      }
      break;
    default:
      break;
  }
}

void BleV2Medium::WatcherHandler(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementWatcherStoppedEventArgs args) {
  // This method is called when watcher stopped. Args give more detailed
  // information on the reason.
  switch (args.Error()) {
    case BluetoothError::Success:
      LOG(ERROR) << "Nearby BLE Medium stoped to scan successfully.";
      break;
    case BluetoothError::RadioNotAvailable:
      LOG(ERROR)
          << "Nearby BLE Medium stoped to scan due to radio not available.";
      break;
    case BluetoothError::ResourceInUse:
      LOG(ERROR) << "Nearby BLE Medium stoped to scan due to resource in use.";
      break;
    case BluetoothError::DeviceNotConnected:
      LOG(ERROR) << "Nearby BLE Medium stoped to scan due to "
                    "remote device is not connected.";
      break;
    case BluetoothError::DisabledByPolicy:
      LOG(ERROR)
          << "Nearby BLE Medium stoped to scan due to disabled by policy.";
      break;
    case BluetoothError::DisabledByUser:
      LOG(ERROR) << "Nearby BLE Medium stoped to scan due to disabled by user.";
      break;
    case BluetoothError::NotSupported:
      LOG(ERROR)
          << "Nearby BLE Medium stoped to scan due to hardware not supported.";
      break;
    case BluetoothError::TransportNotSupported:
      LOG(ERROR) << "Nearby BLE Medium stoped to scan due to "
                    "transport not supported.";
      break;
    case BluetoothError::ConsentRequired:
      LOG(ERROR) << "Nearby BLE Medium stoped to scan due to consent required.";
      break;
    case BluetoothError::OtherError:
      LOG(ERROR) << "Nearby BLE Medium stoped to scan due to unknown errors.";
      break;
    default:
      LOG(ERROR) << "Nearby BLE Medium stoped to scan due to unknown errors.";
      break;
  }
}

void BleV2Medium::AdvertisementReceivedHandler(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementReceivedEventArgs args) {
  // Handle all BLE advertisements and determine whether the BLE Medium
  // Advertisement Scan Response packet (containing Copresence UUID 0xFEF3 in
  // 0x16 Service Data) has been received in the handler
  BluetoothLEAdvertisement advertisement = args.Advertisement();
  std::string bluetooth_address =
      uint64_to_mac_address_string(args.BluetoothAddress());
  bool has_primary_service_data = false;
  std::vector<std::string> alt_service_ids;

  for (BluetoothLEAdvertisementDataSection service_data :
       advertisement.GetSectionsByType(kUuid16ServiceDataType)) {
    // Parse Advertisement Data for Section 0x16 (16bit uuid Service Data)
    DataReader data_reader = DataReader::FromBuffer(service_data.Data());
    data_reader.ByteOrder(ByteOrder::LittleEndian);

    // Discard the first 2 bytes of Service Uuid in Service Data
    uint16_t service_uuid16 = data_reader.ReadUInt16();

    if (service_uuid16 == service_uuid16_) {
      std::string data;

      uint8_t unconsumed_buffer_length = data_reader.UnconsumedBufferLength();
      for (int i = 0; i < unconsumed_buffer_length; i++) {
        data.append(1, static_cast<unsigned char>(data_reader.ReadByte()));
      }

      ByteArray advertisement_data(data);

      VLOG(1) << "Nearby BLE Medium " << absl::StrCat(absl::Hex(service_uuid16))
              << " Advertisement discovered. "
                 "0x16 Service data: advertisement bytes= 0x"
              << absl::BytesToHexString(advertisement_data.AsStringView())
              << "(" << advertisement_data.size() << ")";

      BleV2Peripheral* peripheral_ptr = nullptr;
      {
        absl::MutexLock lock(&mutex_);
        peripheral_ptr = GetOrCreatePeripheral(bluetooth_address);
        if (peripheral_ptr == nullptr) {
          LOG(ERROR) << "No BLE peripheral with address: " << bluetooth_address;
          return;
        }
      }
      LOG(INFO) << "BLE peripheral with address: " << bluetooth_address;

      // Received Advertisement packet
      LOG(INFO) << "unconsumed_buffer_length: "
                << static_cast<int>(unconsumed_buffer_length);

      api::ble_v2::BleAdvertisementData ble_advertisement_data;
      if (unconsumed_buffer_length <= 27) {
        ble_advertisement_data.is_extended_advertisement = false;
      } else {
        ble_advertisement_data.is_extended_advertisement = true;
      }

      ble_advertisement_data.service_data[service_uuid_] = advertisement_data;

      has_primary_service_data = true;
      scan_callback_.advertisement_found_cb(*peripheral_ptr,
                                            ble_advertisement_data);
    } else {
      absl::MutexLock lock(&mutex_);
      const auto& alternate_it =
          alternate_uuids_for_service_.find(service_uuid16);
      if (alternate_it != alternate_uuids_for_service_.end()) {
        alt_service_ids.push_back(alternate_it->second);
      }
    }
  }
  // Only process alternate service data if there is no primary service data.
  if (!has_primary_service_data && !alt_service_ids.empty()) {
    BleV2Peripheral* peripheral_ptr = nullptr;
    {
      absl::MutexLock lock(&mutex_);
      peripheral_ptr = GetOrCreatePeripheral(bluetooth_address);
      if (peripheral_ptr == nullptr) {
        LOG(ERROR) << "No BLE peripheral with address: " << bluetooth_address;
        return;
      }
    }
    LOG(INFO) << "Found BLE peripheral for with address: " << bluetooth_address
              << " for services: " << absl::StrJoin(alt_service_ids, ",");
    // Create fake advertisement data.
    api::ble_v2::BleAdvertisementData ble_advertisement_data;
    ble_advertisement_data.is_extended_advertisement = false;
    BleAdvertisementHeader header =
        CreateAdvertisementHeader(bluetooth_address, alt_service_ids);
    ble_advertisement_data.service_data[service_uuid_] = ByteArray(header);
    scan_callback_.advertisement_found_cb(*peripheral_ptr,
                                          ble_advertisement_data);
  }
}

void BleV2Medium::AdvertisementFoundHandler(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementReceivedEventArgs args) {
  BluetoothLEAdvertisement advertisement = args.Advertisement();
  std::vector<Uuid> service_uuid_list;
  bool found_matching_service_uuid = false;
  {
    absl::MutexLock lock(&mutex_);
    for (auto windows_service_uuid : advertisement.ServiceUuids()) {
      auto nearby_service_uuid =
          winrt_guid_to_nearby_uuid(windows_service_uuid);
      service_uuid_list.push_back(nearby_service_uuid);
      if (service_uuid_to_session_map_.find(nearby_service_uuid) !=
          service_uuid_to_session_map_.end()) {
        found_matching_service_uuid = true;
      }
    }
  }
  if (!found_matching_service_uuid) {
    return;
  }

  // Construct BleAdvertisementData from the windows Advertisement.
  api::ble_v2::BleAdvertisementData ble_advertisement_data;
  for (BluetoothLEAdvertisementDataSection service_data :
       advertisement.GetSectionsByType(kUuid16ServiceDataType)) {
    // Parse Advertisement Data for Section 0x16 (Service Data)
    DataReader data_reader = DataReader::FromBuffer(service_data.Data());
    data_reader.ByteOrder(ByteOrder::LittleEndian);

    // Discard the first 2 bytes of Service Uuid in Service Data
    uint16_t service_uuid16 = data_reader.ReadUInt16();

    for (auto service_uuid : service_uuid_list) {
      std::optional<uint16_t> uuid16 = service_uuid.GetBtUuid16();
      if (!uuid16.has_value()) {
        continue;
      }
      if (*uuid16 == service_uuid16) {
        std::string data;

        uint8_t unconsumed_buffer_length = data_reader.UnconsumedBufferLength();
        if (unconsumed_buffer_length > 27) {
          LOG(INFO) << "Skipping extended advertisement with service "
                    << absl::StrCat(absl::Hex(*uuid16));
          return;
        }
        for (int i = 0; i < unconsumed_buffer_length; i++) {
          data.append(1, static_cast<unsigned char>(data_reader.ReadByte()));
        }
        ble_advertisement_data.service_data[service_uuid] = ByteArray{data};
      }
    }
  }
  if (ble_advertisement_data.service_data.empty()) {
    LOG(ERROR) << "Got matching Service UUID but found no "
                  "corresponding data, skipping";
    return;
  }
  // Save the BleV2Peripheral.
  std::string bluetooth_address =
      uint64_to_mac_address_string(args.BluetoothAddress());
  BleV2Peripheral* peripheral_ptr = nullptr;
  {
    absl::MutexLock lock(&mutex_);
    peripheral_ptr = GetOrCreatePeripheral(bluetooth_address);
    if (peripheral_ptr == nullptr) {
      LOG(ERROR) << "No BLE peripheral with address: " << bluetooth_address;
      return;
    }
  }
  LOG(INFO) << "BLE peripheral with address: " << bluetooth_address;

  // Invokes callbacks that matches the UUID.
  for (auto service_uuid : service_uuid_list) {
    {
      absl::MutexLock lock(&mutex_);
      if (service_uuid_to_session_map_.find(service_uuid) !=
          service_uuid_to_session_map_.end()) {
        for (auto& id_session_pair :
             service_uuid_to_session_map_[service_uuid]) {
          id_session_pair.second.advertisement_found_cb(*peripheral_ptr,
                                                        ble_advertisement_data);
        }
      }
    }
  }
}

bool BleV2Medium::GetRemotePeripheral(const std::string& mac_address,
                                      GetRemotePeripheralCallback callback) {
  BleV2Peripheral* peripheral = nullptr;
  {
    absl::MutexLock lock(&mutex_);
    peripheral = GetOrCreatePeripheral(mac_address);
  }

  if (peripheral != nullptr && peripheral->Ok()) {
    callback(*peripheral);
    return true;
  }
  return false;
}

bool BleV2Medium::GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                                      GetRemotePeripheralCallback callback) {
  BleV2Peripheral* peripheral = nullptr;
  {
    absl::MutexLock lock(&mutex_);
    peripheral = GetPeripheral(id);
  }

  if (peripheral == nullptr) {
    LOG(WARNING) << __func__ << ": No matched peripheral device.";
    return false;
  }
  callback(*peripheral);
  return true;
}

uint64_t BleV2Medium::GenerateSessionId() {
  for (int i = 0; i < kGenerateSessionIdRetryLimit; i++) {
    uint64_t session_id = Prng().NextInt64();
    if (session_id == kFailedGenerateSessionId) continue;
    for (auto& element : service_uuid_to_session_map_) {
      if (element.second.find(session_id) != element.second.end()) continue;
    }
    return session_id;
  }
  return kFailedGenerateSessionId;
}

BleV2Peripheral* BleV2Medium::GetOrCreatePeripheral(absl::string_view address) {
  auto it = std::find_if(
      peripheral_map_.begin(), peripheral_map_.end(), [&](const auto& item) {
        return item.second.peripheral->GetAddress() == address;
      });
  if (it != peripheral_map_.end()) {
    it->second.last_access_time = absl::Now();
    return it->second.peripheral.get();
  }
  RemoveExpiredPeripherals();
  PeripheralInfo peripheral_info{
      .last_access_time = absl::Now(),
      .peripheral = std::make_unique<BleV2Peripheral>(address),
  };
  BleV2Peripheral* peripheral = peripheral_info.peripheral.get();
  if (!peripheral->Ok()) {
    LOG(WARNING) << __func__ << "Invalid MAC address: " << address;
    return nullptr;
  }
  LOG(INFO) << "New BLE peripheral with address: " << address;

  peripheral_map_[peripheral->GetUniqueId()] = std::move(peripheral_info);
  return peripheral;
}

BleV2Peripheral* BleV2Medium::GetPeripheral(BleV2Peripheral::UniqueId id) {
  auto it = peripheral_map_.find(id);
  if (it == peripheral_map_.end()) {
    return nullptr;
  }
  it->second.last_access_time = absl::Now();
  return it->second.peripheral.get();
}

void BleV2Medium::RemoveExpiredPeripherals() {
  absl::Time now = absl::Now();
  if (cleanup_time_ + kMaxPeripheralCleanupFrequency < now) {
    return;
  }
  cleanup_time_ = now;
  absl::Time cut_off_time = now - kPeripheralExpiryTime;
  for (auto it = peripheral_map_.begin(); it != peripheral_map_.end();) {
    if (it->second.last_access_time < cut_off_time) {
      peripheral_map_.erase(it++);
    } else {
      ++it;
    }
  }
}

void BleV2Medium::AddAlternateUuidForService(uint16_t uuid,
                                             const std::string& service_id) {
  absl::MutexLock lock(&mutex_);
  alternate_uuids_for_service_[uuid] = service_id;
}

}  // namespace windows
}  // namespace nearby
