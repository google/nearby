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

#include <combaseapi.h>
// #include <propkey.h>
#include <iostream>
#include <memory>
#include <string>

#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/gatt_server.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/logging.h"
#include "winrt/Windows.Devices.Bluetooth.Advertisement.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Foundation.Collections.h"

namespace location {
namespace nearby {
namespace windows {

namespace {

using ::location::nearby::api::ble_v2::AdvertiseParameters;
using ::location::nearby::api::ble_v2::BleAdvertisementData;
using ::location::nearby::api::ble_v2::BleServerSocket;
using ::location::nearby::api::ble_v2::BleSocket;
using ::location::nearby::api::ble_v2::GattClient;
using ::location::nearby::api::ble_v2::ServerGattConnectionCallback;
using ::location::nearby::api::ble_v2::TxPowerLevel;
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

std::string BleV2Peripheral::GetAddress() const { return ""; }

BleV2Medium::BleV2Medium(api::BluetoothAdapter& adapter)
    : adapter_(dynamic_cast<BluetoothAdapter*>(&adapter)) {}

BleV2Medium::~BleV2Medium() {}

// advertisement packet and populate accordingly
bool BleV2Medium::StartAdvertising(const BleAdvertisementData& advertising_data,
                                   AdvertiseParameters advertising_parameters) {
  NEARBY_LOGS(INFO)
      << "Windows Ble StartAdvertising:, advertising_data.service_data size="
      << advertising_data.service_data.size() << ", tx_power_level="
      << TxPowerLevelToName(advertising_parameters.tx_power_level);

  if (advertising_data.service_data.empty()) return false;

  absl::MutexLock lock(&mutex_);

  gatt_server_->StartAdvertising();

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
  // Additional properties we would like about the device.
  // Property strings are documented here
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ff521659(v=vs.85).aspx
  auto requestedProperties = winrt::single_threaded_vector<winrt::hstring>(
      {L"System.Devices.Aep.DeviceAddress", L"System.Devices.Aep.IsConnected",
       L"System.Devices.Aep.Bluetooth.Le.IsConnectable",
       L"System.Devices.AepService.Bluetooth.ServiceGuid"});

  // BT_Code: Example showing paired and non-paired in a single query.
  winrt::hstring aqsAllBluetoothLEDevices =
      L"(System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-"
      L"76eaa7084d49}\")";

  deviceWatcher =
      winrt::Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(
          aqsAllBluetoothLEDevices, requestedProperties,
          DeviceInformationKind::AssociationEndpoint);

  // Register event handlers before starting the watcher.
  deviceWatcherAddedToken =
      deviceWatcher.Added({this, &BleV2Medium::DeviceWatcher_Added});
  deviceWatcherUpdatedToken =
      deviceWatcher.Updated({this, &BleV2Medium::DeviceWatcher_Updated});
  deviceWatcherRemovedToken =
      deviceWatcher.Removed({this, &BleV2Medium::DeviceWatcher_Removed});
  deviceWatcherEnumerationCompletedToken = deviceWatcher.EnumerationCompleted(
      {this, &BleV2Medium::DeviceWatcher_EnumerationCompleted});
  deviceWatcherStoppedToken =
      deviceWatcher.Stopped({this, &BleV2Medium::DeviceWatcher_Stopped});

  // Start over with an empty collection.
  // m_knownDevices.Clear();

  // Start the watcher. Active enumeration is limited to approximately 30
  // seconds. This limits power usage and reduces interference with other
  // Bluetooth activities. To monitor for the presence of Bluetooth LE devices
  // for an extended period, use the BluetoothLEAdvertisementWatcher runtime
  // class. See the BluetoothAdvertisement sample for an example.
  deviceWatcher.Start();
  return true;
}

winrt::fire_and_forget BleV2Medium::DeviceWatcher_Added(
    DeviceWatcher sender, DeviceInformation deviceInfo) {
  OutputDebugStringW((L"Added " + deviceInfo.Id() + deviceInfo.Name()).c_str());

  // GUID result =
  //     deviceInfo.Properties()[PKEY_DeviceInterface_Bluetooth_ServiceGuid];

  // auto _service =
  // winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDeviceService::FromIdAsync(deviceInfo.Id()).get();
  auto btdev =
      winrt::Windows::Devices::Bluetooth::BluetoothLEDevice::FromIdAsync(
          deviceInfo.Id())
          .get();
  auto result = btdev.GetGattServicesAsync().get();

  if (result.Status() ==
      winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCommunicationStatus::Success) {
    for (auto service_instance : result.Services()) {
      auto myuuid = service_instance.Uuid();
      OLECHAR guidbuffer[64];
      StringFromGUID2(myuuid, guidbuffer, 64);
      char buffer[500];

      // First arg is the pointer to destination char, second arg is
      // the pointer to source wchar_t, last arg is the size of char buffer
      wcstombs(buffer, guidbuffer, 500);

      NEARBY_LOGS(INFO) << "Found service %s" << buffer;
    }
  }
  // Protect against race condition if the task runs after the app stopped the
  // deviceWatcher.
  if (sender == deviceWatcher) {
    // Make sure device isn't already present in the list.
    // if (std::get<0>(FindBluetoothLEDeviceDisplay(deviceInfo.Id())) ==
    // nullptr) {
    //  if (!deviceInfo.Name().empty()) {
    //    // If device has a friendly name display it immediately.
    //    m_knownDevices.Append(make<BluetoothLEDeviceDisplay>(deviceInfo));
    //  } else {
    //    // Add it to a list in case the name gets updated later.
    //    UnknownDevices.push_back(deviceInfo);
    //  }
  }
  return winrt::fire_and_forget();
}

winrt::fire_and_forget BleV2Medium::DeviceWatcher_Updated(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  OutputDebugStringW((L"Updated " + deviceInfoUpdate.Id()).c_str());

  // Protect against race condition if the task runs after the app stopped the
  // deviceWatcher.
  if (sender == deviceWatcher) {
    // SDKTemplate::BluetoothLEDeviceDisplay bleDeviceDisplay =
    //     std::get<0>(FindBluetoothLEDeviceDisplay(deviceInfoUpdate.Id()));
    // if (bleDeviceDisplay != nullptr) {
    //   // Device is already being displayed - update UX.
    //   bleDeviceDisplay.Update(deviceInfoUpdate);
    //   co_return;
  }

  // auto deviceInfo = FindUnknownDevices(deviceInfoUpdate.Id());
  // if (deviceInfo != UnknownDevices.end()) {
  //   deviceInfo->Update(deviceInfoUpdate);
  //   // If device has been updated with a friendly name it's no longer
  //   unknown. if (!deviceInfo->Name().empty()) {
  //     m_knownDevices.Append(make<BluetoothLEDeviceDisplay>(*deviceInfo));
  //     UnknownDevices.erase(deviceInfo);
  //   }
  // }
  return winrt::fire_and_forget();
}

winrt::fire_and_forget BleV2Medium::DeviceWatcher_Removed(
    winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
    winrt::Windows::Devices::Enumeration::DeviceInformationUpdate
        deviceInfoUpdate) {
  OutputDebugStringW((L"Removed " + deviceInfoUpdate.Id()).c_str());

  // Protect against race condition if the task runs after the app stopped the
  // deviceWatcher.
  if (sender == deviceWatcher) {
    // Find the corresponding DeviceInformation in the collection and remove it.
    // auto [bleDeviceDisplay, index] =
    //    FindBluetoothLEDeviceDisplay(deviceInfoUpdate.Id());
    // if (bleDeviceDisplay != nullptr) {
    //  m_knownDevices.RemoveAt(index);
    //}

    // auto deviceInfo = FindUnknownDevices(deviceInfoUpdate.Id());
    // if (deviceInfo != UnknownDevices.end()) {
    //   UnknownDevices.erase(deviceInfo);
    // }
  }
  return winrt::fire_and_forget();
}

winrt::fire_and_forget BleV2Medium::DeviceWatcher_EnumerationCompleted(
    winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
    winrt::Windows::Foundation::IInspectable const&) {
  // Protect against race condition if the task runs after the app stopped the
  // deviceWatcher.
  if (sender == deviceWatcher) {
    // rootPage.NotifyUser(to_hstring(m_knownDevices.Size()) +
    //                         L" devices found. Enumeration completed.",
    //                     NotifyType::StatusMessage);
  }
  return winrt::fire_and_forget();
}

winrt::fire_and_forget BleV2Medium::DeviceWatcher_Stopped(
    winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
    winrt::Windows::Foundation::IInspectable const&) {
  // Protect against race condition if the task runs after the app stopped the
  // deviceWatcher.
  if (sender == deviceWatcher) {
    // rootPage.NotifyUser(L"No longer watching for devices.",
    //                     sender.Status() == DeviceWatcherStatus::Aborted
    //                         ? NotifyType::ErrorMessage
    //                         : NotifyType::StatusMessage);
  }
  return winrt::fire_and_forget();
}

bool BleV2Medium::StopScanning() {
  NEARBY_LOGS(INFO) << "Windows Ble StopScanning";
  absl::MutexLock lock(&mutex_);

  watcher_stopped_callback_ = [this]() {
    scanning_stopped_ = true;
    scanning_started_ = false;
  };
  watcher_error_callback_ = [this]() {
    scanning_error_ = true;
    scanning_stopped_ = false;
    scanning_started_ = false;
  };

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
  std::unique_ptr<GattServer> gatt_server =
      std::make_unique<GattServer>(callback);

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
  return nullptr;
}

std::unique_ptr<api::ble_v2::BleSocket> BleV2Medium::Connect(
    const std::string& service_id, TxPowerLevel tx_power_level,
    api::ble_v2::BlePeripheral& remote_peripheral,
    CancellationFlag* cancellation_flag) {
  return nullptr;
}

void BleV2Medium::PublisherHandler(
    BluetoothLEAdvertisementPublisher publisher,
    BluetoothLEAdvertisementPublisherStatusChangedEventArgs args) {
  absl::MutexLock lock(&mutex_);
  switch (args.Status()) {
    case BluetoothLEAdvertisementPublisherStatus::Started:
      publisher_started_callback_();
      break;
    case BluetoothLEAdvertisementPublisherStatus::Stopped:
      publisher_stopped_callback_();
      publisher_.StatusChanged(publisher_token_);
      break;
    case BluetoothLEAdvertisementPublisherStatus::Aborted:
      switch (args.Error()) {
        case BluetoothError::RadioNotAvailable:
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "radio not available.";
          break;
        case BluetoothError::ResourceInUse:
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "resource in use.";
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
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "unknown errors.";
          break;
        default:
          NEARBY_LOGS(ERROR) << "Nearby BLE Medium advertising failed due to "
                                "unknown errors.";
          break;
      }
      publisher_error_callback_();
      break;
    default:
      break;
  }
}

void BleV2Medium::WatcherHandler(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementWatcherStoppedEventArgs args) {
  absl::MutexLock lock(&mutex_);
  switch (args.Error()) {
    case BluetoothError::RadioNotAvailable:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to radio not available.";
      break;
    case BluetoothError::ResourceInUse:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to resource in use.";
      break;
    case BluetoothError::DisabledByPolicy:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to disabled by policy.";
      break;
    case BluetoothError::DisabledByUser:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to disabled by user.";
      break;
    case BluetoothError::NotSupported:
      NEARBY_LOGS(ERROR) << "Nearby BLE Medium scanning failed due to "
                            "hardware not supported.";
      break;
    case BluetoothError::TransportNotSupported:
      NEARBY_LOGS(ERROR) << "Nearby BLE Medium scanning failed due to "
                            "transport not supported.";
      break;
    case BluetoothError::ConsentRequired:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to consent required.";
      break;
    case BluetoothError::OtherError:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to unknown errors.";
      break;
    case BluetoothError::Success:
      if (watcher_stopped_callback_ != nullptr &&
          watcher_.Status() == BluetoothLEAdvertisementWatcherStatus::Started) {
        watcher_started_callback_();
      }
      if (watcher_stopped_callback_ != nullptr &&
          watcher_.Status() == BluetoothLEAdvertisementWatcherStatus::Stopped) {
        watcher_stopped_callback_();
        watcher_.Stopped(watcher_token_);
        watcher_.Received(advertisement_received_token_);
      }
    default:
      NEARBY_LOGS(ERROR)
          << "Nearby BLE Medium scanning failed due to unknown errors.";
      break;
  }
}

void BleV2Medium::AdvertisementReceivedHandler(
    BluetoothLEAdvertisementWatcher watcher,
    BluetoothLEAdvertisementReceivedEventArgs args) {
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

  absl::MutexLock lock(&mutex_);

  if (args.IsScanResponse()) {
    IVector<winrt::guid> guids = args.Advertisement().ServiceUuids();
    bool scan_response_found = false;
    for (const winrt::guid& uuid : guids) {
      if (uuid == kCopresenceServiceUuid128bit) {
        scan_response_found = true;
      }
    }
    if (scan_response_found == true) {
      BleV2Peripheral peripheral;
      api::ble_v2::BleAdvertisementData advertisement_data;
      scan_response_received_callback_.advertisement_found_cb(
          peripheral, advertisement_data);
    }
  }
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
