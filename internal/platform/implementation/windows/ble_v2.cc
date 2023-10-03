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
#include <array>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/escaping.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/ble_gatt_client.h"
#include "internal/platform/implementation/windows/ble_gatt_server.h"
#include "internal/platform/implementation/windows/ble_v2_server_socket.h"
#include "internal/platform/implementation/windows/ble_v2_socket.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/prng.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Storage.Streams.h"  // NOLINT(misc-include-cleaner)

namespace nearby {
namespace windows {

namespace {

using ::nearby::api::ble_v2::AdvertiseParameters;
using ::nearby::api::ble_v2::BleAdvertisementData;
using ::nearby::api::ble_v2::BleServerSocket;
using ::nearby::api::ble_v2::BleSocket;
using ::nearby::api::ble_v2::GattClient;
using ::nearby::api::ble_v2::ServerGattConnectionCallback;
using ::nearby::api::ble_v2::TxPowerLevel;
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
using ::winrt::Windows::Storage::Streams::Buffer;
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

// Max times trying to generate unused session id.
static constexpr uint64_t kGenerateSessionIdRetryLimit = 3;
// Indicating failed to generate unused session id.
static constexpr uint64_t kFailedGenerateSessionId = 0;

// Remove lost/unused peripherals after a timeout.
constexpr absl::Duration kPeripheralExpiryTime = absl::Minutes(15);
// Prevent too frequent cleanup tasks.
constexpr absl::Duration kMaxPeripheralCleanupFrequency = absl::Minutes(3);
}  // namespace

BleV2Medium::BleV2Medium(api::BluetoothAdapter& adapter)
    : adapter_(dynamic_cast<BluetoothAdapter*>(&adapter)) {}

// Advertisement packet and populate accordingly.
bool BleV2Medium::StartAdvertising(const BleAdvertisementData& advertising_data,
                                   AdvertiseParameters advertising_parameters) {
  std::string service_data_info;
  for (const auto& it : advertising_data.service_data) {
    service_data_info += "{uuid:" + std::string(it.first) +
                         ",data size:" + absl::StrCat(it.second.size()) +
                         ", data=0x" +
                         absl::BytesToHexString(it.second.AsStringView()) + "}";
  }

  NEARBY_LOGS(INFO) << __func__
                    << ": advertising_data.service_data=" << service_data_info
                    << ", tx_power_level="
                    << TxPowerLevelToName(
                           advertising_parameters.tx_power_level);

  if (advertising_data.is_extended_advertisement) {
    // In BLE v2, the flag is set when the Bluetooth adapter supports extended
    // advertising and GATT server is using.
    NEARBY_LOGS(INFO) << __func__
                      << ": BLE advertising using BLE extended feature.";
    return StartBleAdvertising(advertising_data, advertising_parameters);
  } else {
    if (ble_gatt_server_ != nullptr) {
      NEARBY_LOGS(INFO) << __func__ << ": BLE advertising on GATT server.";
      return StartGattAdvertising(advertising_data, advertising_parameters);
    } else {
      NEARBY_LOGS(INFO) << __func__ << ": BLE fast advertising.";
      return StartBleAdvertising(advertising_data, advertising_parameters);
    }
  }
}

bool BleV2Medium::StopAdvertising() {
  NEARBY_LOGS(INFO) << __func__ << ": Stop advertising.";
  bool result;
  if (is_gatt_publisher_started_) {
    bool stop_gatt_result = StopGattAdvertising();
    if (!stop_gatt_result) {
      NEARBY_LOGS(WARNING) << "Failed to stop GATT advertising.";
    }
    ble_gatt_server_ = nullptr;
    result = stop_gatt_result;
  }

  if (is_ble_publisher_started_) {
    bool stop_ble_result = StopBleAdvertising();
    if (!stop_ble_result) {
      NEARBY_LOGS(WARNING) << "Failed to stop BLE advertising.";
    }
    result = result && stop_ble_result;
  }

  return result;
}

std::unique_ptr<BleV2Medium::AdvertisingSession> BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_parameters,
    BleV2Medium::AdvertisingCallback callback) {
  NEARBY_LOGS(INFO) << __func__
                    << ": advertising_data.is_extended_advertisement="
                    << advertising_data.is_extended_advertisement
                    << ", advertising_data.service_data size="
                    << advertising_data.service_data.size()
                    << ", tx_power_level="
                    << TxPowerLevelToName(advertise_parameters.tx_power_level)
                    << ", is_connectable="
                    << advertise_parameters.is_connectable;
  // TODO(hais): add real impl for windows StartAdvertising.
  return nullptr;
}

bool BleV2Medium::StartScanning(const Uuid& service_uuid,
                                TxPowerLevel tx_power_level,
                                ScanCallback callback) {
  NEARBY_LOGS(INFO) << __func__
                    << ": service UUID: " << std::string(service_uuid)
                    << ", TxPowerLevel: " << TxPowerLevelToName(tx_power_level);
  try {
    if (!adapter_->IsEnabled()) {
      NEARBY_LOGS(WARNING) << __func__
                           << "BLE cannot start scanning because the "
                              "Bluetooth adapter is not enabled.";
      return false;
    }

    if (is_watcher_started_) {
      NEARBY_LOGS(WARNING)
          << __func__ << ": BLE cannot start to scan again when it is running.";
      return false;
    }

    service_uuid_ = service_uuid;
    tx_power_level_ = tx_power_level;
    scan_callback_ = std::move(callback);

    watcher_ = BluetoothLEAdvertisementWatcher();
    watcher_token_ = watcher_.Stopped({this, &BleV2Medium::WatcherHandler});
    advertisement_received_token_ =
        watcher_.Received({this, &BleV2Medium::AdvertisementReceivedHandler});

    if (adapter_->IsExtendedAdvertisingSupported()) {
      watcher_.AllowExtendedAdvertisements(true);
    }

    // Active mode indicates that scan request packets will be sent to query for
    // Scan Response
    watcher_.ScanningMode(BluetoothLEScanningMode::Active);

    BluetoothLEAdvertisementDataSection data_section;
    data_section.DataType(0x16);
    DataWriter data_writer;  // NOLINT(misc-include-cleaner)
    std::array<char, 16> service_id_data = service_uuid_.data();
    data_writer.WriteByte(service_id_data[3] & 0xff);
    data_writer.WriteByte(service_id_data[2] & 0xff);
    data_section.Data(data_writer.DetachBuffer());
    BluetoothLEAdvertisement advertisement;
    advertisement.DataSections().Append(data_section);
    BluetoothLEAdvertisementFilter advertisement_filter;
    advertisement_filter.Advertisement(advertisement);
    watcher_.AdvertisementFilter(advertisement_filter);
    watcher_.Start();

    is_watcher_started_ = true;

    NEARBY_LOGS(INFO) << __func__ << ": BLE scanning started.";
    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception to start BLE scanning: "
                       << exception.what();

    return false;
  } catch (const winrt::hresult_error& ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Exception to start BLE scanning: " << ex.code()
                       << ": " << winrt::to_string(ex.message());

    return false;
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
    return false;
  }
}

std::unique_ptr<BleV2Medium::ScanningSession> BleV2Medium::StartScanning(
    const Uuid& service_uuid, TxPowerLevel tx_power_level,
    BleV2Medium::ScanningCallback callback) {
  NEARBY_LOGS(INFO) << __func__
                    << ": service UUID: " << std::string(service_uuid)
                    << ", TxPowerLevel: " << TxPowerLevelToName(tx_power_level);
  if (!adapter_->IsEnabled()) {
    NEARBY_LOGS(WARNING) << __func__
                         << "BLE cannot start scanning because the "
                            "Bluetooth adapter is not enabled.";
    return nullptr;
  }
  if (!is_watcher_started_) {
    NEARBY_LOGS(WARNING) << __func__ << ": Starting BLE Scanning.";
    try {
      watcher_ = BluetoothLEAdvertisementWatcher();
      watcher_token_ = watcher_.Stopped({this, &BleV2Medium::WatcherHandler});
      advertisement_received_token_ =
          watcher_.Received({this, &BleV2Medium::AdvertisementFoundHandler});

      if (adapter_->IsExtendedAdvertisingSupported()) {
        watcher_.AllowExtendedAdvertisements(true);
      } else {
        watcher_.AllowExtendedAdvertisements(false);
      }

      // Active mode indicates that scan request packets will be sent to query
      // for Scan Response
      watcher_.ScanningMode(BluetoothLEScanningMode::Active);
      ::winrt::Windows::Devices::Bluetooth::BluetoothSignalStrengthFilter
          filter;
      filter.SamplingInterval(TimeSpan(std::chrono::seconds(2)));
      watcher_.SignalStrengthFilter(filter);
      watcher_.Start();
      is_watcher_started_ = true;
      NEARBY_LOGS(INFO) << __func__ << ": BLE scanning started.";
    } catch (std::exception exception) {
      NEARBY_LOGS(ERROR) << __func__ << ": Exception to start BLE scanning: "
                         << exception.what();
      return nullptr;
    } catch (const winrt::hresult_error& ex) {
      NEARBY_LOGS(ERROR) << __func__
                         << ": Exception to start BLE scanning: " << ex.code()
                         << ": " << winrt::to_string(ex.message());
      return nullptr;
    } catch (...) {
      NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
      return nullptr;
    }
  } else {
    NEARBY_LOGS(WARNING) << __func__ << ": BLE Scanning already started.";
  }
  uint64_t session_id = GenerateSessionId();

  // Save session id, service id and callback for this scan session.
  {
    absl::MutexLock lock(&map_mutex_);
    auto iter = service_uuid_to_session_map_.find(service_uuid);
    if (iter == service_uuid_to_session_map_.end()) {
      service_uuid_to_session_map_[service_uuid].insert(
          {session_id, std::move(callback)});
    } else {
      iter->second.insert({session_id, std::move(callback)});
    }
  }

  // Generate and return ScanningSession.
  return std::make_unique<BleV2Medium::ScanningSession>(
      BleV2Medium::ScanningSession{
          .stop_scanning =
              [this, session_id, service_uuid]() {
                size_t num_erased_from_service_and_session_map = 0u;
                {
                  absl::MutexLock lock(&map_mutex_);
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
                      NEARBY_LOGS(INFO)
                          << "No more scan sessions, stopping Ble scanning.";
                      watcher_.Stop();
                      is_watcher_started_ = false;

                      NEARBY_LOGS(INFO) << "Ble stoped scanning successfully.";
                    } catch (std::exception exception) {
                      NEARBY_LOGS(ERROR)
                          << __func__ << ": Exception to stop BLE scanning: "
                          << exception.what();
                      return absl::InternalError(
                          "Bad status stopping Ble scanning");
                    } catch (const winrt::hresult_error& ex) {
                      NEARBY_LOGS(ERROR)
                          << __func__
                          << ": Exception to stop BLE scanning: " << ex.code()
                          << ": " << winrt::to_string(ex.message());
                      return absl::InternalError(
                          "Bad status stopping Ble scanning");
                    } catch (...) {
                      NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
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
  NEARBY_LOGS(INFO) << __func__ << ": Start GATT server.";

  if (!NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableBleV2Gatt)) {
    if (adapter_->IsExtendedAdvertisingSupported()) {
      NEARBY_LOGS(WARNING) << __func__ << ": GATT is disabled.";
      return nullptr;
    }

    if (!NearbyFlags::GetInstance().GetBoolFlag(
            platform::config_package_nearby::nearby_platform_feature::
                kEnableBleV2GattOnNonExtendedDevice)) {
      NEARBY_LOGS(WARNING) << __func__ << ": GATT is disabled.";
      return nullptr;
    }
  }
  auto gatt_server =
      std::make_unique<BleGattServer>(adapter_, std::move(callback));

  ble_gatt_server_ = gatt_server.get();

  return gatt_server;
}

std::unique_ptr<api::ble_v2::GattClient> BleV2Medium::ConnectToGattServer(
    api::ble_v2::BlePeripheral& peripheral, TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  NEARBY_LOGS(INFO) << "ConnectToGattServer is called, address: "
                    << peripheral.GetAddress()
                    << ", power:" << TxPowerLevelToName(tx_power_level);

  if (!NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableBleV2Gatt)) {
    if (adapter_->IsExtendedAdvertisingSupported()) {
      NEARBY_LOGS(WARNING) << __func__ << ": GATT is disabled.";
      return nullptr;
    }

    if (!NearbyFlags::GetInstance().GetBoolFlag(
            platform::config_package_nearby::nearby_platform_feature::
                kEnableBleV2GattOnNonExtendedDevice)) {
      NEARBY_LOGS(WARNING) << __func__ << ": GATT is disabled.";
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
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
  }

  return nullptr;
}

bool BleV2Medium::StopScanning() {
  NEARBY_LOGS(INFO) << __func__ << ": BLE StopScanning: service_uuid: "
                    << std::string(service_uuid_);
  try {
    if (!adapter_->IsEnabled()) {
      NEARBY_LOGS(WARNING) << "BLE cannot stop scanning because the "
                              "bluetooth adapter is not enabled.";
      return false;
    }

    if (!is_watcher_started_) {
      NEARBY_LOGS(WARNING) << "BLE scanning is not running.";
      return false;
    }

    watcher_.Stop();

    // Don't need to wait for the status becomes to `Stopped`. If application
    // starts to scanning immediately, the scanning still needs to wait the
    // stopping to finish.
    is_watcher_started_ = false;

    NEARBY_LOGS(ERROR)
        << "Windows Ble stoped scanning successfully for service UUID:"
        << std::string(service_uuid_);
    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception to stop BLE scanning: "
                       << exception.what();

    return false;
  } catch (const winrt::hresult_error& ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Exception to stop BLE scanning: " << ex.code()
                       << ": " << winrt::to_string(ex.message());

    return false;
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
    return false;
  }
}

std::unique_ptr<api::ble_v2::BleServerSocket> BleV2Medium::OpenServerSocket(
    const std::string& service_id) {
  NEARBY_LOGS(INFO) << "OpenServerSocket is called";

  auto server_socket = std::make_unique<BleV2ServerSocket>(adapter_);

  if (!server_socket->Bind()) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to bing socket.";
    return nullptr;
  }

  return server_socket;
}

std::unique_ptr<api::ble_v2::BleSocket> BleV2Medium::Connect(
    const std::string& service_id, TxPowerLevel tx_power_level,
    api::ble_v2::BlePeripheral& remote_peripheral,
    CancellationFlag* cancellation_flag) {
  NEARBY_LOGS(INFO) << __func__ << ": Connect to service_id=" << service_id;

  if (cancellation_flag == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": cancellation_flag not specified.";
    return nullptr;
  }

  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(INFO) << __func__
                      << ": BLE socket connection cancelled for service: "
                      << service_id;
    return nullptr;
  }

  auto ble_socket = std::make_unique<BleV2Socket>();

  nearby::CancellationFlagListener cancellation_flag_listener(
      cancellation_flag, [socket = ble_socket.get()]() { socket->Close(); });

  if (!ble_socket->Connect(&remote_peripheral)) {
    NEARBY_LOGS(INFO) << __func__
                      << ": BLE socket connection failed. service_id="
                      << service_id;
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
  NEARBY_LOGS(INFO) << __func__ << ": Start BLE advertising.";
  try {
    if (!adapter_->IsEnabled()) {
      NEARBY_LOGS(WARNING) << "BLE cannot start advertising because the "
                              "bluetooth adapter is not enabled.";
      return false;
    }

    if (advertising_data.service_data.empty()) {
      NEARBY_LOGS(WARNING)
          << "BLE cannot start to advertise due to invalid service data.";
      return false;
    }

    if (is_ble_publisher_started_) {
      NEARBY_LOGS(WARNING)
          << "BLE cannot start to advertise again when it is running.";
      return false;
    }

    BluetoothLEAdvertisement advertisement;
    IVector<BluetoothLEAdvertisementDataSection> data_sections =
        advertisement.DataSections();

    int max_data_section_size = 0;

    for (const auto& it : advertising_data.service_data) {
      DataWriter data_writer;

      std::string uuid_string = it.first.Get16BitAsString();
      int uuid;
      if (!absl::SimpleHexAtoi(uuid_string, &uuid)) {
        NEARBY_LOGS(WARNING) << "BLE failed to get service UUID.";
        return false;
      }

      NEARBY_LOGS(WARNING) << "BLE service UUID: "
                           << absl::StrFormat("%#x", uuid);

      data_writer.WriteUInt16(((uuid >> 8) & 0xff) | ((uuid & 0xff) << 8));

      for (int i = 0; i < it.second.size(); ++i) {
        data_writer.WriteByte(static_cast<uint8_t>(*(it.second.data() + i)));
      }

      if (max_data_section_size < it.second.size()) {
        max_data_section_size = it.second.size();
      }

      BluetoothLEAdvertisementDataSection ble_service_data =
          BluetoothLEAdvertisementDataSection(0x16, data_writer.DetachBuffer());

      data_sections.Append(ble_service_data);
    }

    advertisement.DataSections() = data_sections;

    // Use Extended Advertising if Fast Advertisement Service Uuid is empty
    // string because the long format advertisement will be used
    if (advertising_data.is_extended_advertisement) {
      if (!adapter_->IsExtendedAdvertisingSupported()) {
        NEARBY_LOGS(WARNING)
            << "Cannot advertise extended advertisement on devie without BLE "
               "advertisement extention feature.";
        return false;
      }

      publisher_ = BluetoothLEAdvertisementPublisher(advertisement);
      publisher_.UseExtendedAdvertisement(true);
    } else {
      if (max_data_section_size > 27) {
        NEARBY_LOGS(WARNING) << "Invalid advertisement data size for "
                                "non-extended advertisement.";
        return false;
      }

      publisher_ = BluetoothLEAdvertisementPublisher(advertisement);
      publisher_.UseExtendedAdvertisement(false);
    }
    publisher_token_ =
        publisher_.StatusChanged({this, &BleV2Medium::PublisherHandler});

    publisher_.Start();

    is_ble_publisher_started_ = true;
    NEARBY_LOGS(INFO) << "BLE advertising started.";
    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception to start BLE advertising: "
                       << exception.what();

    return false;
  } catch (const winrt::hresult_error& ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Exception to start BLE advertising: " << ex.code()
                       << ": " << winrt::to_string(ex.message());

    return false;
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
    return false;
  }
}

bool BleV2Medium::StopBleAdvertising() {
  NEARBY_LOGS(INFO) << __func__ << ": Stop BLE advertising.";
  try {
    if (!adapter_->IsEnabled()) {
      NEARBY_LOGS(WARNING) << "BLE cannot stop advertising because the "
                              "bluetooth adapter is not enabled.";
      return false;
    }

    if (!is_ble_publisher_started_) {
      NEARBY_LOGS(WARNING) << "BLE advertising is not running.";
      return false;
    }

    // publisher_ may be null when status changed during advertising.
    if (publisher_ != nullptr &&
        publisher_.Status() ==
            BluetoothLEAdvertisementPublisherStatus::Started) {
      publisher_.Stop();
    }

    // Don't need to wait for the status becomes to `Stopped`. If application
    // starts to scanning immediately, the scanning still needs to wait the
    // stopping to finish.
    is_ble_publisher_started_ = false;

    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception to stop BLE advertising: "
                       << exception.what();

    return false;
  } catch (const winrt::hresult_error& ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Exception to stop BLE advertising: " << ex.code()
                       << ": " << winrt::to_string(ex.message());

    return false;
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
    return false;
  }
}

bool BleV2Medium::StartGattAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertising_parameters) {
  NEARBY_LOGS(INFO) << __func__ << ": Start GATT advertising.";
  try {
    if (!adapter_->IsEnabled()) {
      NEARBY_LOGS(WARNING) << "BLE cannot start advertising because the "
                              "bluetooth adapter is not enabled.";
      return false;
    }

    if (advertising_data.service_data.empty()) {
      NEARBY_LOGS(WARNING)
          << "BLE cannot start to advertise due to invalid service data.";
      return false;
    }

    if (is_gatt_publisher_started_) {
      NEARBY_LOGS(WARNING)
          << "BLE cannot start to advertise again when it is running.";
      return false;
    }

    if (ble_gatt_server_ == nullptr) {
      NEARBY_LOGS(WARNING) << "No Gatt server is running.";
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
      NEARBY_LOGS(WARNING) << "BLE cannot start to advertise.";
      return false;
    }

    is_gatt_publisher_started_ = true;

    NEARBY_LOGS(INFO) << "GATT advertising started.";
    return true;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception to start GATT advertising: "
                       << exception.what();

    return false;
  } catch (const winrt::hresult_error& ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Exception to start GATT advertising: " << ex.code()
                       << ": " << winrt::to_string(ex.message());

    return false;
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
    return false;
  }
}

bool BleV2Medium::StopGattAdvertising() {
  try {
    NEARBY_LOGS(INFO) << __func__ << ": Stop GATT advertising.";
    if (!adapter_->IsEnabled()) {
      NEARBY_LOGS(WARNING) << "BLE cannot stop advertising because the "
                              "bluetooth adapter is not enabled.";
      return false;
    }

    if (!is_gatt_publisher_started_) {
      NEARBY_LOGS(WARNING) << "BLE advertising is not running.";
      return false;
    }

    if (ble_gatt_server_ == nullptr) {
      NEARBY_LOGS(WARNING) << "No Gatt server is running.";
      return false;
    }

    bool stop_result = ble_gatt_server_->StopAdvertisement();
    is_gatt_publisher_started_ = false;

    NEARBY_LOGS(INFO) << "Stop GATT advertisement result=" << stop_result;
    return stop_result;
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception to stop BLE advertising: "
                       << exception.what();

    return false;
  } catch (const winrt::hresult_error& ex) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Exception to stop BLE advertising: " << ex.code()
                       << ": " << winrt::to_string(ex.message());

    return false;
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exception.";
    return false;
  }
}

void BleV2Medium::PublisherHandler(
    BluetoothLEAdvertisementPublisher publisher,
    BluetoothLEAdvertisementPublisherStatusChangedEventArgs args) {
  // This method is called when publisher's status is changed.
  switch (args.Status()) {
    case BluetoothLEAdvertisementPublisherStatus::Created:
      NEARBY_LOGS(INFO) << "Nearby BLE Medium created to advertise.";
      return;
    case BluetoothLEAdvertisementPublisherStatus::Started:
      NEARBY_LOGS(INFO) << "Nearby BLE Medium started to advertise.";
      return;
    case BluetoothLEAdvertisementPublisherStatus::Stopping:
      NEARBY_LOGS(INFO) << "Nearby BLE Medium is stopping.";
      return;
    case BluetoothLEAdvertisementPublisherStatus::Waiting:
      NEARBY_LOGS(INFO) << "Nearby BLE Medium is waiting.";
      return;
    case BluetoothLEAdvertisementPublisherStatus::Stopped:
      NEARBY_LOGS(INFO) << "Nearby BLE Medium stopped to advertise.";
      break;
    case BluetoothLEAdvertisementPublisherStatus::Aborted:
      switch (args.Error()) {
        case BluetoothError::Success:
          if (publisher_.Status() ==
              BluetoothLEAdvertisementPublisherStatus::Started) {
            NEARBY_LOGS(ERROR)
                << "Nearby BLE Medium start advertising operation was "
                   "successfully completed or serviced.";
          }
          if (publisher_.Status() ==
              BluetoothLEAdvertisementPublisherStatus::Stopped) {
            NEARBY_LOGS(ERROR)
                << "Nearby BLE Medium stop advertising operation was "
                   "successfully completed or serviced.";
          } else {
            NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                  "unknown errors.";
          }
          break;
        case BluetoothError::RadioNotAvailable:
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "radio not available.";
          break;
        case BluetoothError::ResourceInUse:
          NEARBY_LOGS(ERROR)
              << "Nearby BLE Medium advertising failed due to resource in use.";
          break;
        case BluetoothError::DeviceNotConnected:
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "remote device is not connected.";
          break;
        case BluetoothError::DisabledByPolicy:
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "disabled by policy.";
          break;
        case BluetoothError::DisabledByUser:
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "disabled by user.";
          break;
        case BluetoothError::NotSupported:
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "hardware not supported.";
          break;
        case BluetoothError::TransportNotSupported:
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "transport not supported.";
          break;
        case BluetoothError::ConsentRequired:
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "consent required.";
          break;
        case BluetoothError::OtherError:
        default:
          NEARBY_LOGS(ERROR)
              << "Nearby BLE Medium advertising failed due to unknown errors.";
          break;
      }
      break;
    default:
      break;
  }

  // The publisher is stopped. Clean up the running publisher
  if (publisher_ != nullptr) {
    NEARBY_LOGS(ERROR) << "Nearby BLE Medium cleaned the publisher.";
    publisher_.StatusChanged(publisher_token_);
    publisher_ = nullptr;
    is_ble_publisher_started_ = false;
  }
}

void BleV2Medium::WatcherHandler(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementWatcherStoppedEventArgs args) {
  // This method is called when watcher stopped. Args give more detailed
  // information on the reason.
  switch (args.Error()) {
    case BluetoothError::Success:
      NEARBY_LOGS(ERROR) << "Nearby BLE Medium stoped to scan successfully.";
      break;
    case BluetoothError::RadioNotAvailable:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium stoped to scan due to radio not available.";
      break;
    case BluetoothError::ResourceInUse:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium stoped to scan due to resource in use.";
      break;
    case BluetoothError::DeviceNotConnected:
      NEARBY_LOGS(ERROR) << "Nearby BLE Medium stoped to scan due to "
                            "remote device is not connected.";
      break;
    case BluetoothError::DisabledByPolicy:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium stoped to scan due to disabled by policy.";
      break;
    case BluetoothError::DisabledByUser:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium stoped to scan due to disabled by user.";
      break;
    case BluetoothError::NotSupported:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium stoped to scan due to hardware not supported.";
      break;
    case BluetoothError::TransportNotSupported:
      NEARBY_LOGS(ERROR) << "Nearby BLE Medium stoped to scan due to "
                            "transport not supported.";
      break;
    case BluetoothError::ConsentRequired:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium stoped to scan due to consent required.";
      break;
    case BluetoothError::OtherError:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium stoped to scan due to unknown errors.";
      break;
    default:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium stoped to scan due to unknown errors.";
      break;
  }

  // No matter the reason, should clean up the watcher if it is not empty.
  // The BLE V1 interface doesn't have API to return the error to upper layer.
  if (watcher_ != nullptr) {
    NEARBY_LOGS(ERROR) << "Nearby BLE Medium cleaned the watcher.";
    watcher_.Stopped(watcher_token_);
    watcher_.Received(advertisement_received_token_);
    watcher_ = nullptr;
    is_watcher_started_ = false;
  }
}

void BleV2Medium::AdvertisementReceivedHandler(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementReceivedEventArgs args) {
  // Handle all BLE advertisements and determine whether the BLE Medium
  // Advertisement Scan Response packet (containing Copresence UUID 0xFEF3 in
  // 0x16 Service Data) has been received in the handler
  BluetoothLEAdvertisement advertisement = args.Advertisement();

  std::array<char, 16> service_id_data = service_uuid_.data();
  for (BluetoothLEAdvertisementDataSection service_data :
       advertisement.GetSectionsByType(0x16)) {
    // Parse Advertisement Data for Section 0x16 (Service Data)
    DataReader data_reader = DataReader::FromBuffer(service_data.Data());

    // Discard the first 2 bytes of Service Uuid in Service Data
    uint8_t first_byte = data_reader.ReadByte();
    uint8_t second_byte = data_reader.ReadByte();

    if (first_byte == (service_id_data[3] & 0xff) &&
        second_byte == (service_id_data[2] & 0xff)) {
      std::string data;

      uint8_t unconsumed_buffer_length = data_reader.UnconsumedBufferLength();
      for (int i = 0; i < unconsumed_buffer_length; i++) {
        data.append(1, static_cast<unsigned char>(data_reader.ReadByte()));
      }

      ByteArray advertisement_data(data);

      NEARBY_LOGS(VERBOSE) << "Nearby BLE Medium "
                           << service_uuid_.Get16BitAsString()
                           << " Advertisement discovered. "
                              "0x16 Service data: advertisement bytes= 0x"
                           << absl::BytesToHexString(
                                  advertisement_data.AsStringView())
                           << "(" << advertisement_data.size() << ")";

      std::string bluetooth_address =
          uint64_to_mac_address_string(args.BluetoothAddress());
      BleV2Peripheral* peripheral_ptr =
          GetOrCreatePeripheral(bluetooth_address);
      if (peripheral_ptr == nullptr) {
        NEARBY_LOGS(ERROR) << "No BLE peripheral with address: "
                           << bluetooth_address;
        return;
      }
      NEARBY_LOGS(INFO) << "BLE peripheral with address: " << bluetooth_address;

      // Received Advertisement packet
      NEARBY_LOGS(INFO) << "unconsumed_buffer_length: "
                        << static_cast<int>(unconsumed_buffer_length);

      api::ble_v2::BleAdvertisementData ble_advertisement_data;
      if (unconsumed_buffer_length <= 27) {
        ble_advertisement_data.is_extended_advertisement = false;
      } else {
        ble_advertisement_data.is_extended_advertisement = true;
      }

      ble_advertisement_data.service_data[service_uuid_] = advertisement_data;

      scan_callback_.advertisement_found_cb(*peripheral_ptr,
                                            ble_advertisement_data);
    }
  }
}

void BleV2Medium::AdvertisementFoundHandler(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementReceivedEventArgs args) {
  BluetoothLEAdvertisement advertisement = args.Advertisement();
  std::vector<Uuid> service_uuid_list;
  bool found_matching_service_uuid = false;
  {
    absl::MutexLock lock(&map_mutex_);
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
       advertisement.GetSectionsByType(0x16)) {
    // Parse Advertisement Data for Section 0x16 (Service Data)
    DataReader data_reader = DataReader::FromBuffer(service_data.Data());

    // Discard the first 2 bytes of Service Uuid in Service Data
    uint8_t first_byte = data_reader.ReadByte();
    uint8_t second_byte = data_reader.ReadByte();

    for (auto service_uuid : service_uuid_list) {
      std::array<char, 16> service_id_data = service_uuid.data();
      if (first_byte == (service_id_data[3] & 0xff) &&
          second_byte == (service_id_data[2] & 0xff)) {
        std::string data;

        uint8_t unconsumed_buffer_length = data_reader.UnconsumedBufferLength();
        if (unconsumed_buffer_length > 27) {
          NEARBY_LOGS(INFO) << "Skipping extended advertisement with service "
                            << service_uuid.Get16BitAsString();
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
    NEARBY_LOGS(ERROR) << "Got matching Service UUID but found no "
                          "corresponding data, skipping";
    return;
  }
  // Save the BleV2Peripheral.
  std::string bluetooth_address =
      uint64_to_mac_address_string(args.BluetoothAddress());
  BleV2Peripheral* peripheral_ptr = GetOrCreatePeripheral(bluetooth_address);
  if (peripheral_ptr == nullptr) {
    NEARBY_LOGS(ERROR) << "No BLE peripheral with address: "
                       << bluetooth_address;
    return;
  }
  NEARBY_LOGS(INFO) << "BLE peripheral with address: " << bluetooth_address;

  // Invokes callbacks that matches the UUID.
  for (auto service_uuid : service_uuid_list) {
    {
      absl::MutexLock lock(&map_mutex_);
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
  BleV2Peripheral* peripheral = GetOrCreatePeripheral(mac_address);
  if (peripheral != nullptr && peripheral->Ok()) {
    callback(*peripheral);
    return true;
  }
  return false;
}

bool BleV2Medium::GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId id,
                                      GetRemotePeripheralCallback callback) {
  BleV2Peripheral* peripheral = GetPeripheral(id);
  if (peripheral == nullptr) {
    NEARBY_LOGS(WARNING) << __func__ << ": No matched peripheral device.";
    return false;
  }
  callback(*peripheral);
  return true;
}

uint64_t BleV2Medium::GenerateSessionId() {
  absl::MutexLock lock(&map_mutex_);
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
  absl::MutexLock lock(&peripheral_map_mutex_);
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
    NEARBY_LOGS(WARNING) << __func__ << "Invalid MAC address: " << address;
    return nullptr;
  }
  NEARBY_LOGS(INFO) << "New BLE peripheral with address: " << address;

  peripheral_map_[peripheral->GetUniqueId()] = std::move(peripheral_info);
  return peripheral;
}

BleV2Peripheral* BleV2Medium::GetPeripheral(BleV2Peripheral::UniqueId id) {
  absl::MutexLock lock(&peripheral_map_mutex_);
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

}  // namespace windows
}  // namespace nearby
