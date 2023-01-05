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

#include "internal/platform/implementation/windows/mediums_manager/mediums_manager_impl.h"

#include <exception>

#include "absl/strings/string_view.h"
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

// Provides data for a Received event on a BluetoothLEAdvertisementWatcher. A
// BluetoothLEAdvertisementReceivedEventArgs instance is created when the
// Received event occurs on a BluetoothLEAdvertisementWatcher object.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementreceivedeventargs?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementReceivedEventArgs;

// Writes data to an output stream.
// https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.datawriter?view=winrt-22621
using ::winrt::Windows::Storage::Streams::DataWriter;

template <typename T>
using IVector = winrt::Windows::Foundation::Collections::IVector<T>;

// Copresence Service UUID 0xfef3 (little-endian)
constexpr uint16_t kCopresenceServiceUuid = 0xf3fe;
}  // namespace

bool MediumsManager::StartBleAdvertising(
    absl::string_view service_id, const ByteArray& advertisement_bytes,
    absl::string_view fast_advertisement_service_uuid) {
  try {
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
        publisher_.StatusChanged({this, &MediumsManager::PublisherHandler});

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

bool MediumsManager::StopBleAdvertising(absl::string_view service_id) {
  try {
    if (!is_publisher_started_) {
      NEARBY_LOGS(WARNING) << "BLE advertising is not running.";
      return false;
    }

    publisher_.Stop();

    // Don't need to wait for the status becomes to `Stopped`. If application
    // starts to scanning immediately, the scanning still needs to wait the
    // stopping to finish.
    is_publisher_started_ = false;

    NEARBY_LOGS(INFO) << "Windows Ble Stopped advertising: service_id="
                      << service_id;

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

void MediumsManager::PublisherHandler(
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

}  // namespace windows
}  // namespace nearby
