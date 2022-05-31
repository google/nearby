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

#include <future>  // NOLINT
#include <string>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Storage.Streams.h"

namespace location {
namespace nearby {
namespace windows {
namespace {
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
using ::winrt::Windows::Storage::Streams::DataWriter;

template <typename T>
using IVector = winrt::Windows::Foundation::Collections::IVector<T>;

// Copresence Service UUID 0xfef3 (little-endian)
constexpr uint16_t kCopresenceServiceUuid = 0xf3fe;
}  // namespace

BleMedium::BleMedium(api::BluetoothAdapter& adapter)
    : adapter_(static_cast<api::BluetoothAdapter*>(&adapter)) {}

bool BleMedium::StartAdvertising(
    const std::string& service_id, const ByteArray& advertisement_bytes,
    const std::string& fast_advertisement_service_uuid) {
  absl::MutexLock lock(&mutex_);

  NEARBY_LOGS(INFO) << "Windows Ble StartAdvertising: service_id=" << service_id
                    << ", advertisement bytes=" << advertisement_bytes.data()
                    << "(" << advertisement_bytes.size() << "),"
                    << " fast advertisement service uuid="
                    << fast_advertisement_service_uuid;

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

  IVector<BluetoothLEAdvertisementDataSection> data_sections =
      advertisement_.DataSections();
  data_sections.Append(service_data);
  advertisement_.DataSections() = data_sections;

  // Use Extended Advertising if Fast Advertisement Service Uuid is empty string
  // because the long format advertisement will be used
  if (fast_advertisement_service_uuid.empty()) {
    publisher_ = BluetoothLEAdvertisementPublisher(advertisement_);
    publisher_.UseExtendedAdvertisement(true);

    publisher_token_ =
        publisher_.StatusChanged({this, &BleMedium::PublisherHandler});

    publisher_started_promise_ = std::promise<PublisherState>();

    std::future<PublisherState> publisher_state_future =
        publisher_started_promise_.get_future();

    publisher_.Start();

    return publisher_state_future.get() == PublisherState::kStarted;
  } else {
    // Extended Advertisement not supported, must make sure advertisement_bytes
    // is less than 27 bytes
    if (advertisement_bytes.size() <= 27) {
      publisher_ = BluetoothLEAdvertisementPublisher(advertisement_);
      publisher_.UseExtendedAdvertisement(false);

      publisher_token_ =
          publisher_.StatusChanged({this, &BleMedium::PublisherHandler});

      publisher_started_promise_ = std::promise<PublisherState>();

      std::future<PublisherState> publisher_state_future =
          publisher_started_promise_.get_future();

      publisher_.Start();

      return publisher_state_future.get() == PublisherState::kStarted;
    } else {
      // otherwise no-op
      NEARBY_LOGS(INFO) << "Everyone Mode unavailable for hardware that does "
                           "not support Extended Advertising.";
      return false;
    }
  }
}

bool BleMedium::StopAdvertising(const std::string& service_id) {
  absl::MutexLock lock(&mutex_);

  NEARBY_LOGS(INFO) << "Windows Ble StopAdvertising: service_id=" << service_id;

  publisher_stopped_promise_ = std::promise<PublisherState>();

  std::future<PublisherState> publisher_state_future =
      publisher_stopped_promise_.get_future();

  publisher_.Stop();

  return publisher_state_future.get() == PublisherState::kStopped;
}

// TODO(b/234229557): Add BlePeripheral discovery logic
bool BleMedium::StartScanning(
    const std::string& service_id,
    const std::string& fast_advertisement_service_uuid,
    DiscoveredPeripheralCallback callback) {
  absl::MutexLock lock(&mutex_);

  NEARBY_LOGS(INFO) << "Windows Ble StartScanning: service_id=" << service_id;

  advertisement_received_callback_ = std::move(callback);

  watcher_token_ = watcher_.Stopped({this, &BleMedium::WatcherHandler});
  advertisement_received_token_ =
      watcher_.Received({this, &BleMedium::AdvertisementReceivedHandler});

  watcher_started_promise_ = std::promise<WatcherState>();

  std::future<WatcherState> watcher_state_future =
      watcher_started_promise_.get_future();

  // Active mode indicates that scan request packets will be sent to query for
  // Scan Response
  watcher_.ScanningMode(BluetoothLEScanningMode::Active);
  watcher_.Start();

  while (!is_watcher_started_) {
    if (watcher_.Status() == BluetoothLEAdvertisementWatcherStatus::Created) {
      watcher_started_promise_.set_value(WatcherState::kStarted);
      return true;
    }
  }
  return true;
}

bool BleMedium::StopScanning(const std::string& service_id) {
  absl::MutexLock lock(&mutex_);
  NEARBY_LOGS(INFO) << "Windows Ble StopScanning: service_id=" << service_id;

  watcher_stopped_promise_ = std::promise<WatcherState>();

  std::future<WatcherState> watcher_state_future =
      watcher_stopped_promise_.get_future();

  watcher_.Stop();

  while (!is_watcher_stopped_) {
    if (watcher_.Status() == BluetoothLEAdvertisementWatcherStatus::Stopped) {
      watcher_stopped_promise_.set_value(WatcherState::kStopped);
      watcher_.Stopped(watcher_token_);
      watcher_.Received(advertisement_received_token_);
      return true;
    }
  }
  return true;
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
  switch (args.Status()) {
    case BluetoothLEAdvertisementPublisherStatus::Started:
      publisher_started_promise_.set_value(PublisherState::kStarted);
      break;
    case BluetoothLEAdvertisementPublisherStatus::Stopped:
      publisher_stopped_promise_.set_value(PublisherState::kStopped);
      publisher_.StatusChanged(publisher_token_);
      break;
    case BluetoothLEAdvertisementPublisherStatus::Aborted:
      switch (args.Error()) {
        case BluetoothError::RadioNotAvailable:
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "radio not available.";
          break;
        case BluetoothError::ResourceInUse:
          NEARBY_LOGS(ERROR)
              << "Nearby BLE Medium advertising failed due to resource in use.";
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
      publisher_started_promise_.set_value(PublisherState::kError);
      publisher_stopped_promise_.set_value(PublisherState::kError);
      break;
    default:
      break;
  }
}

void BleMedium::WatcherHandler(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementWatcherStoppedEventArgs args) {
  switch (args.Error()) {
    case BluetoothError::RadioNotAvailable:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to radio not available.";
      watcher_started_promise_.set_value(WatcherState::kError);
      watcher_stopped_promise_.set_value(WatcherState::kError);
      break;
    case BluetoothError::ResourceInUse:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to resource in use.";
      watcher_started_promise_.set_value(WatcherState::kError);
      watcher_stopped_promise_.set_value(WatcherState::kError);
      break;
    case BluetoothError::DisabledByPolicy:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to disabled by policy.";
      watcher_started_promise_.set_value(WatcherState::kError);
      watcher_stopped_promise_.set_value(WatcherState::kError);
      break;
    case BluetoothError::DisabledByUser:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to disabled by user.";
      watcher_started_promise_.set_value(WatcherState::kError);
      watcher_stopped_promise_.set_value(WatcherState::kError);
      break;
    case BluetoothError::NotSupported:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to hardware not supported.";
      watcher_started_promise_.set_value(WatcherState::kError);
      watcher_stopped_promise_.set_value(WatcherState::kError);
      break;
    case BluetoothError::TransportNotSupported:
      NEARBY_LOGS(ERROR) << "Nearby BLE Medium scanning failed due to "
                            "transport not supported.";
      watcher_started_promise_.set_value(WatcherState::kError);
      watcher_stopped_promise_.set_value(WatcherState::kError);
      break;
    case BluetoothError::ConsentRequired:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to consent required.";
      watcher_started_promise_.set_value(WatcherState::kError);
      watcher_stopped_promise_.set_value(WatcherState::kError);
      break;
    case BluetoothError::OtherError:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to unknown errors.";
      watcher_started_promise_.set_value(WatcherState::kError);
      watcher_stopped_promise_.set_value(WatcherState::kError);
      break;
    default:
      if (watcher_.Status() == BluetoothLEAdvertisementWatcherStatus::Started) {
        watcher_started_promise_.set_value(WatcherState::kStarted);
        is_watcher_started_ = true;
      }
      if (watcher_.Status() == BluetoothLEAdvertisementWatcherStatus::Stopped) {
        watcher_stopped_promise_.set_value(WatcherState::kStopped);
        watcher_.Stopped(watcher_token_);
        watcher_.Received(advertisement_received_token_);
        is_watcher_stopped_ = true;
      } else {
        NEARBY_LOGS(ERROR)
            << "Nearby BLE Medium scanning failed due to unknown errors.";
        watcher_started_promise_.set_value(WatcherState::kError);
        watcher_stopped_promise_.set_value(WatcherState::kError);
      }
      break;
  }
}

void BleMedium::AdvertisementReceivedHandler(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementReceivedEventArgs args) {
  // Handle all BLE advertisements and determine whether the BLE Medium
  // Advertisement Scan Response packet (containing Copresence UUID 0xFEF3) has
  // been received in the handler
  std::array<uint8_t, 8> bluetooth_base_array = {
      static_cast<uint8_t>(0x80), static_cast<uint8_t>(0x00),
      static_cast<uint8_t>(0x00), static_cast<uint8_t>(0x80),
      static_cast<uint8_t>(0x5F), static_cast<uint8_t>(0x9B),
      static_cast<uint8_t>(0x34), static_cast<uint8_t>(0xFB)};
  winrt::guid kCopresenceServiceUuid128bit(
      static_cast<uint32_t>(0x0000FEF3), static_cast<uint16_t>(0x0000),
      static_cast<uint16_t>(0x1000), bluetooth_base_array);

  IVector<winrt::guid> guids = args.Advertisement().ServiceUuids();
  bool is_advertisement_found = false;
  for (const winrt::guid& uuid : guids) {
    if (uuid == kCopresenceServiceUuid128bit) {
      is_advertisement_found = true;
    }
  }
  if (is_advertisement_found == true) {
    BlePeripheral peripheral;
    advertisement_received_callback_.peripheral_discovered_cb(peripheral,
                                                              "\xfe\xf3", true);
  }
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
