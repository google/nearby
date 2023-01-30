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

#include "internal/platform/implementation/windows/ble_medium.h"

#include <chrono>  // NOLINT
#include <exception>
#include <future>  // NOLINT
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/escaping.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/windows/ble_peripheral.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Storage.Streams.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {
namespace {

// Specifies common Bluetooth error cases.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetootherror?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::BluetoothError;

// Represents a Bluetooth LE advertisement payload.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisement?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisement;

// Represents a Bluetooth LE advertisement section. A Bluetooth LE advertisement
// packet can contain multiple instances of these
// BluetoothLEAdvertisementDataSection objects.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementdatasection?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataSection;

// Represents the Bluetooth LE advertisement types defined in the Generic Access
// Profile (GAP) by the Bluetooth Special Interest Group (SIG).
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementdatatypes?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataTypes;

// Represents an object to send Bluetooth Low Energy (LE) advertisements.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementpublisher?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisher;

// Represents the possible states of the BluetoothLEAdvertisementPublisher.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementpublisherstatus?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatus;

// Provides data for a StatusChanged event on a
// BluetoothLEAdvertisementPublisher.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementpublisherstatuschangedeventargs?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementPublisherStatusChangedEventArgs;

// BluetoothLEAdvertisement
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisement?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisement;

// Provides data for a Received event on a BluetoothLEAdvertisementWatcher. A
// BluetoothLEAdvertisementReceivedEventArgs instance is created when the
// Received event occurs on a BluetoothLEAdvertisementWatcher object.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementreceivedeventargs?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementReceivedEventArgs;

// Represents an object to receive Bluetooth Low Energy (LE) advertisements.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcher?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcher;

// Represents the possible states of the BluetoothLEAdvertisementWatcher.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcherstatus?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStatus;

// Provides data for a Stopped event on a BluetoothLEAdvertisementWatcher. A
// BluetoothLEAdvertisementWatcherStoppedEventArgs instance is created when the
// Stopped event occurs on a BluetoothLEAdvertisementWatcher object.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcherstoppedeventargs?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStoppedEventArgs;

// Defines constants that specify a Bluetooth LE scanning mode.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothlescanningmode?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEScanningMode;

// Reads data from an input stream.
// https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.datareader?view=winrt-22621
using ::winrt::Windows::Storage::Streams::DataReader;

// Writes data to an output stream.
// https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.datawriter?view=winrt-22621
using ::winrt::Windows::Storage::Streams::DataWriter;

// Represents a time interval as a signed 64-bit integer value.
// https://docs.microsoft.com/en-us/uwp/api/windows.foundation.timespan?view=winrt-22621
using ::winrt::Windows::Foundation::TimeSpan;

template <typename T>
using IVector = winrt::Windows::Foundation::Collections::IVector<T>;

// Copresence Service UUID 0xfef3 (little-endian)
constexpr uint16_t kCopresenceServiceUuid = 0xf3fe;
}  // namespace

BleMedium::BleMedium(api::BluetoothAdapter& adapter)
    : adapter_(static_cast<BluetoothAdapter*>(&adapter)) {}

bool BleMedium::StartAdvertising(
    const std::string& service_id, const ByteArray& advertisement_bytes,
    const std::string& fast_advertisement_service_uuid) {
  try {
    if (!adapter_->IsEnabled()) {
      NEARBY_LOGS(WARNING) << "BLE cannot start advertising because the "
                              "bluetooth adapter is not enabled.";
      return false;
    }

    NEARBY_LOGS(INFO)
        << "Windows Ble StartAdvertising: service_id=" << service_id
        << ", advertisement bytes= 0x"
        << absl::BytesToHexString(advertisement_bytes.AsStringView()) << "("
        << advertisement_bytes.size() << "),"
        << " fast advertisement service uuid= 0x"
        << absl::BytesToHexString(fast_advertisement_service_uuid);

    if (is_publisher_started_) {
      NEARBY_LOGS(WARNING)
          << "BLE cannot start to advertise again when it is running.";
      return false;
    }

    DataWriter data_writer;

    // TODO(b/234229562): Add parsing logic for fast_advertisement_service_uuid
    // and insert into the 0x16 Service Data field in the BLE advertisement when
    // Fast Advertisement is enabled. For Extended Advertising, use the same
    // hardcoded Copresence service uuid 0xFEF3.

    // Copresence Service UUID 0xfef3 (little-endian)
    data_writer.WriteUInt16(kCopresenceServiceUuid);

    for (int i = 0; i < advertisement_bytes.size(); ++i) {
      data_writer.WriteByte(
          static_cast<uint8_t>(*(advertisement_bytes.data() + i)));
    }

    BluetoothLEAdvertisementDataSection service_data =
        BluetoothLEAdvertisementDataSection(0x16, data_writer.DetachBuffer());

    BluetoothLEAdvertisement advertisement;

    IVector<BluetoothLEAdvertisementDataSection> data_sections =
        advertisement.DataSections();

    data_sections.Append(service_data);
    advertisement.DataSections() = data_sections;

    // Use Extended Advertising if Fast Advertisement Service Uuid is empty
    // string because the long format advertisement will be used
    if (fast_advertisement_service_uuid.empty()) {
      publisher_ = BluetoothLEAdvertisementPublisher(advertisement);
      publisher_.UseExtendedAdvertisement(true);
    } else {
      // Extended Advertisement not supported, must make sure
      // advertisement_bytes is less than 27 bytes
      if (advertisement_bytes.size() <= 27) {
        publisher_ = BluetoothLEAdvertisementPublisher(advertisement);
        publisher_.UseExtendedAdvertisement(false);
      } else {
        // otherwise no-op
        NEARBY_LOGS(INFO) << "Everyone Mode unavailable for hardware that does "
                             "not support Extended Advertising.";
        publisher_ = nullptr;
        return false;
      }
    }
    publisher_token_ =
        publisher_.StatusChanged({this, &BleMedium::PublisherHandler});

    publisher_.Start();

    is_publisher_started_ = true;
    NEARBY_LOGS(INFO) << "Windows Ble StartAdvertising started.";
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
  }
}

bool BleMedium::StopAdvertising(const std::string& service_id) {
  try {
    if (!adapter_->IsEnabled()) {
      NEARBY_LOGS(WARNING) << "BLE cannot stop advertising because the "
                              "bluetooth adapter is not enabled.";
      return false;
    }

    NEARBY_LOGS(INFO) << "Windows Ble StopAdvertising: service_id="
                      << service_id;

    if (!is_publisher_started_) {
      NEARBY_LOGS(WARNING) << "BLE advertising is not running.";
      return false;
    }

    publisher_.Stop();

    // Don't need to wait for the status becomes to `Stopped`. If application
    // starts to scanning immediately, the scanning still needs to wait the
    // stopping to finish.
    is_publisher_started_ = false;

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
  }
}

bool BleMedium::StartScanning(
    const std::string& service_id,
    const std::string& fast_advertisement_service_uuid,
    DiscoveredPeripheralCallback callback) {
  try {
    if (!adapter_->IsEnabled()) {
      NEARBY_LOGS(WARNING) << "BLE cannot start scanning because the "
                              "bluetooth adapter is not enabled.";
      return false;
    }

    NEARBY_LOGS(INFO) << "Windows Ble StartScanning: service_id=" << service_id;

    if (is_watcher_started_) {
      NEARBY_LOGS(WARNING)
          << "BLE cannot start to scan again when it is running.";
      return false;
    }

    service_id_ = service_id;
    advertisement_received_callback_ = std::move(callback);
    {
      absl::MutexLock lock(&peripheral_map_mutex_);
      peripheral_map_.clear();
    }

    watcher_ = BluetoothLEAdvertisementWatcher();
    watcher_token_ = watcher_.Stopped({this, &BleMedium::WatcherHandler});
    advertisement_received_token_ =
        watcher_.Received({this, &BleMedium::AdvertisementReceivedHandler});

    if (adapter_->IsExtendedAdvertisingSupported()) {
      watcher_.AllowExtendedAdvertisements(true);
    }
    // Active mode indicates that scan request packets will be sent to query for
    // Scan Response
    watcher_.ScanningMode(BluetoothLEScanningMode::Active);
    ::winrt::Windows::Devices::Bluetooth::BluetoothSignalStrengthFilter filter;
    filter.SamplingInterval(TimeSpan(std::chrono::seconds(2)));
    watcher_.SignalStrengthFilter(filter);
    watcher_.Start();

    is_watcher_started_ = true;

    NEARBY_LOGS(INFO) << "Windows Ble StartScanning started.";
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
  }
}

bool BleMedium::StopScanning(const std::string& service_id) {
  try {
    if (!adapter_->IsEnabled()) {
      NEARBY_LOGS(WARNING) << "BLE cannot stop scanning because the "
                              "bluetooth adapter is not enabled.";
      return false;
    }

    NEARBY_LOGS(INFO) << "Windows Ble StopScanning: service_id=" << service_id;

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
        << "Windows Ble stoped scanning successfully for service_id="
        << service_id;
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
  }
}

bool BleMedium::StartAcceptingConnections(const std::string& service_id,
                                          AcceptedConnectionCallback callback) {
  NEARBY_LOGS(INFO) << "Windows Ble StartAcceptingConnections: service_id="
                    << service_id;
  return true;
}

bool BleMedium::StopAcceptingConnections(const std::string& service_id) {
  NEARBY_LOGS(INFO) << "Windows Ble StopAcceptingConnections: service_id="
                    << service_id;
  return true;
}

std::unique_ptr<api::BleSocket> BleMedium::Connect(
    api::BlePeripheral& remote_peripheral, const std::string& service_id,
    CancellationFlag* cancellation_flag) {
  if (cancellation_flag->Cancelled()) {
    NEARBY_LOGS(ERROR) << "Windows BLE Connect: Has been cancelled: "
                          "service_id="
                       << service_id;
    return {};
  }

  NEARBY_LOGS(ERROR) << "Windows Ble Connect: Cannot connect over BLE socket. "
                        "service_id="
                     << service_id;
  return {};
}

void BleMedium::PublisherHandler(
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
    is_publisher_started_ = false;
  }
}

void BleMedium::WatcherHandler(
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

void BleMedium::AdvertisementReceivedHandler(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementReceivedEventArgs args) {
  // Handle all BLE advertisements and determine whether the BLE Medium
  // Advertisement Scan Response packet (containing Copresence UUID 0xFEF3 in
  // 0x16 Service Data) has been received in the handler
  BluetoothLEAdvertisement advertisement = args.Advertisement();

  for (BluetoothLEAdvertisementDataSection service_data :
       advertisement.GetSectionsByType(0x16)) {
    // Parse Advertisement Data for Section 0x16 (Service Data)
    DataReader data_reader = DataReader::FromBuffer(service_data.Data());

    // Discard the first 2 bytes of Service Uuid in Service Data
    uint8_t first_byte = data_reader.ReadByte();   // 0xf3
    uint8_t second_byte = data_reader.ReadByte();  // 0xfe

    if (first_byte == 0xf3 && second_byte == 0xfe) {
      std::string data;

      uint8_t unconsumed_buffer_length = data_reader.UnconsumedBufferLength();
      for (int i = 0; i < unconsumed_buffer_length; i++) {
        data.append(1, static_cast<unsigned char>(data_reader.ReadByte()));
      }

      ByteArray advertisement_data(data);

      NEARBY_LOGS(VERBOSE)
          << "Nearby BLE Medium 0xFEF3 Advertisement discovered. "
             "0x16 Service data: advertisement bytes= 0x"
          << absl::BytesToHexString(advertisement_data.AsStringView()) << "("
          << advertisement_data.size() << ")";

      std::string peripheral_name =
          uint64_to_mac_address_string(args.BluetoothAddress());

      BlePeripheral* peripheral_ptr = nullptr;

      {
        absl::MutexLock lock(&peripheral_map_mutex_);
        if (peripheral_map_.contains(peripheral_name)) {
          if (peripheral_map_[peripheral_name]->GetAdvertisementBytes(
                  service_id_) != advertisement_data) {
            NEARBY_LOGS(INFO) << "BLE reports lost device: " << peripheral_name;

            // Lost the device first and then report discovered the device.
            advertisement_received_callback_.peripheral_lost_cb(
                /*ble_peripheral*/ *peripheral_map_[peripheral_name],
                /*service_id*/ service_id_);

          } else {
            // The device already reported to discovery, don't need to call it
            // again.
            return;
          }
        }

        auto peripheral = std::make_unique<BlePeripheral>();
        peripheral->SetName(peripheral_name);
        peripheral->SetAdvertisementBytes(advertisement_data);

        peripheral_map_[peripheral_name] = std::move(peripheral);
        peripheral_ptr = peripheral_map_[peripheral_name].get();
      }

      // Received Fast Advertisement packet
      if (unconsumed_buffer_length <= 27) {
        NEARBY_LOGS(INFO)
            << "Sending Fast Advertisement packet for processing.";
        advertisement_received_callback_.peripheral_discovered_cb(
            /*ble_peripheral*/ *peripheral_ptr, /*service_id*/ service_id_,
            /*is_fast_advertisement*/ true);
      } else {
        // Received Extended Advertising packet
        NEARBY_LOGS(INFO)
            << "Sending Extended Advertising packet for processing.";
        advertisement_received_callback_.peripheral_discovered_cb(
            /*ble_peripheral*/ *peripheral_ptr, /*service_id*/ service_id_,
            /*is_fast_advertisement*/ false);
      }
    }
  }
}

}  // namespace windows
}  // namespace nearby
