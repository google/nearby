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

#include "internal/platform/implementation/windows/ble_v2.h"

#include <WTypesbase.h>
#include <combaseapi.h>

#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/ble_gatt_server.h"
#include "internal/platform/implementation/windows/ble_v2_peripheral.h"
#include "internal/platform/implementation/windows/ble_v2_server_socket.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/base.h"

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
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisement;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataSection;
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataTypes;
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
using ::winrt::Windows::Storage::Streams::Buffer;
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

}  // namespace

BleV2Medium::BleV2Medium(api::BluetoothAdapter& adapter)
    : adapter_(dynamic_cast<BluetoothAdapter*>(&adapter)) {}

// advertisement packet and populate accordingly
bool BleV2Medium::StartAdvertising(const BleAdvertisementData& advertising_data,
                                   AdvertiseParameters advertising_parameters) {
  NEARBY_LOGS(INFO)
      << "Windows Ble StartAdvertising:, advertising_data.service_data size="
      << advertising_data.service_data.size() << ", tx_power_level="
      << TxPowerLevelToName(advertising_parameters.tx_power_level);

  if (advertising_data.service_data.empty()) return false;

  absl::MutexLock lock(&mutex_);

  gatt_server_->StartAdvertising(advertising_data, advertising_parameters);

  return true;
}

bool BleV2Medium::StopAdvertising() {
  NEARBY_LOGS(INFO) << "Windows Ble StopAdvertising";
  absl::MutexLock lock(&mutex_);

  if (gatt_server_ != nullptr) {
    gatt_server_->StopAdvertising();
  }
  return true;
}

std::unique_ptr<BleV2Medium::AdvertisingSession> BleV2Medium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_parameters,
    BleV2Medium::AdvertisingCallback callback) {
  NEARBY_LOGS(INFO) << "Windows Ble StartAdvertising: "
                       "advertising_data.is_extended_advertisement="
                    << advertising_data.is_extended_advertisement
                    << ", advertising_data.service_data size="
                    << advertising_data.service_data.size()
                    << ", tx_power_level="
                    << TxPowerLevelToName(advertise_parameters.tx_power_level)
                    << ", is_connectable="
                    << advertise_parameters.is_connectable;
  // TODO(hais): add real impl for windows StartAdvertising.
  return std::make_unique<AdvertisingSession>(AdvertisingSession{});
}

bool BleV2Medium::StartScanning(const Uuid& service_uuid,
                                TxPowerLevel tx_power_level,
                                ScanCallback callback) {
  NEARBY_LOGS(INFO) << "Windows Ble StartScanning";
  absl::MutexLock lock(&mutex_);
  scan_response_received_callback_ = std::move(callback);

  // Handle all BLE advertisements and determine whether the BLE Medium
  // Advertisement Scan Response packet (containing Copresence UUID 0xFEF3)
  // has been received in the handler
  std::array<uint8_t, 8> bluetooth_base_array = {
      static_cast<uint8_t>(0x80), static_cast<uint8_t>(0x00),
      static_cast<uint8_t>(0x00), static_cast<uint8_t>(0x80),
      static_cast<uint8_t>(0x5F), static_cast<uint8_t>(0x9B),
      static_cast<uint8_t>(0x34), static_cast<uint8_t>(0xFB)};
  winrt::guid kCopresenceServiceUuid128bit(
      static_cast<uint32_t>(0x0000FEF3), static_cast<uint16_t>(0x0000),
      static_cast<uint16_t>(0x1000), bluetooth_base_array);

  watcher_.Received(
      [this, kCopresenceServiceUuid128bit](
          BluetoothLEAdvertisementWatcher watcher,
          BluetoothLEAdvertisementReceivedEventArgs eventArgs) {
        auto data = eventArgs.Advertisement();
        auto data_section_count = data.DataSections().Size();
        auto num_services = data.ServiceUuids().Size();

        auto scan_response = false;
        if (eventArgs.IsScanResponse()) {
          NEARBY_LOGS(INFO) << "Got a scan response";
          scan_response = true;
          auto sections = eventArgs.Advertisement().DataSections();
          NEARBY_LOGS(INFO)
              << "Number of Scan Response sections: " << sections.Size();
          NEARBY_LOGS(INFO)
              << "Local name: "
              << winrt::to_string(eventArgs.Advertisement().LocalName());
          NEARBY_LOGS(INFO)
              << "Manufacturer data size: "
              << eventArgs.Advertisement().ManufacturerData().Size();
        }
        for (auto winrt_sevice_uuid : data.ServiceUuids()) {
          if (winrt_sevice_uuid == kCopresenceServiceUuid128bit) {
            NEARBY_LOGS(INFO)
                << "Found found device address: "
                << uint64_to_mac_address_string(eventArgs.BluetoothAddress());

            OLECHAR ole_guid[500];
            // TODO(jfcarroll): Implement this correctly
            StringFromGUID2(winrt_sevice_uuid, ole_guid, 500);

            char uuid[500];

            // First arg is the pointer to destination char, second arg is
            // the pointer to source wchar_t, last arg is the size of char
            // uuid
            wcstombs(uuid, ole_guid, 500);

            NEARBY_LOGS(INFO) << "Found service uuid: " << uuid;

            ::winrt::Windows::Devices::Bluetooth::BluetoothLEDevice bleDevice =
                ::winrt::Windows::Devices::Bluetooth::BluetoothLEDevice::
                    FromBluetoothAddressAsync(eventArgs.BluetoothAddress())
                        .get();

            BleV2Peripheral ble_v2_peripheral(bleDevice);

            auto sections = eventArgs.Advertisement().DataSections();

            NEARBY_LOGS(INFO)
                << "Enumerating " << sections.Size() << " data sections";

            NEARBY_LOGS(INFO) << "==========Start Enumeration===============";

            char buffer[500];
            int buffer_size = 0;

            for (auto section : sections) {
              if (section.DataType() == 0x16) {
                auto section_data = section.Data();

                int index = 0;
                for (; index < section_data.Length(); ++index) {
                  buffer[index] = section_data.data()[index];
                }
                buffer_size = index;
              }

              if (buffer_size > 0) {
                ByteArray advertisement(buffer, buffer_size);
                ::nearby::windows::BleAdvertisementData bleAdvertisementData;
                bleAdvertisementData.service_data.insert(
                    {Uuid(uuid), advertisement});

                // TODO(jfcarroll): Figure out how to populate the advertisement
                // data
                scan_response_received_callback_.advertisement_found_cb(
                    ble_v2_peripheral, bleAdvertisementData);
              }
            }
          }
        }
      });

  watcher_.Stopped(
      [this](BluetoothLEAdvertisementWatcher watcher,
             BluetoothLEAdvertisementWatcherStoppedEventArgs eventArgs) {
        scanning_stopped_ = true;
        scanning_started_ = false;
      });

  // Start the watcher. Active enumeration is limited to approximately 30
  // seconds. This limits power usage and reduces interference with other
  // Bluetooth activities. To monitor for the presence of Bluetooth LE devices
  // for an extended period, use the BluetoothLEAdvertisementWatcher runtime
  // class. See the BluetoothAdvertisement sample for an example.
  watcher_.ScanningMode(::winrt::Windows::Devices::Bluetooth::Advertisement::
                            BluetoothLEScanningMode::Active);
  watcher_.Start();
  scanning_started_ = true;
  scanning_stopped_ = false;
  return true;
}

bool BleV2Medium::StopScanning() {
  NEARBY_LOGS(INFO) << "Windows Ble StopScanning";
  absl::MutexLock lock(&mutex_);

  watcher_.Stop();

  while (!scanning_stopped_) {
    if (scanning_error_) {
      return false;
    }
    if (watcher_.Status() == BluetoothLEAdvertisementWatcherStatus::Stopped) {
      return true;
    }
  }
  return true;
}

std::unique_ptr<BleV2Medium::ScanningSession> BleV2Medium::StartScanning(
    const Uuid& service_uuid, TxPowerLevel tx_power_level,
    BleV2Medium::ScanningCallback callback) {
  NEARBY_LOGS(INFO) << "Windows Ble StartScanning";

  // TODO(hais): add real impl for windows StartAdvertising.
  return std::make_unique<ScanningSession>(ScanningSession{});
}

std::unique_ptr<api::ble_v2::GattServer> BleV2Medium::StartGattServer(
    ServerGattConnectionCallback callback) {
  auto gatt_server = std::make_unique<GattServer>(callback);

  gatt_server_ = gatt_server.get();

  return gatt_server;
}

std::unique_ptr<GattClient> BleV2Medium::ConnectToGattServer(
    api::ble_v2::BlePeripheral& peripheral, TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  return nullptr;
}

std::unique_ptr<api::ble_v2::BleServerSocket> BleV2Medium::OpenServerSocket(
    const std::string& service_id) {
  return std::make_unique<BleV2ServerSocket>(service_id);
}

std::unique_ptr<api::ble_v2::BleSocket> BleV2Medium::Connect(
    const std::string& service_id, TxPowerLevel tx_power_level,
    api::ble_v2::BlePeripheral& remote_peripheral,
    CancellationFlag* cancellation_flag) {
  return nullptr;
}

}  // namespace windows
}  // namespace nearby
