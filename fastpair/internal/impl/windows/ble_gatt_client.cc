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

#include "fastpair/internal/impl/windows/ble_gatt_client.h"

// Standard C/C++ headers
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Nearby headers
#include "absl/types/optional.h"
#include "internal/platform/implementation/windows/generated/winrt/impl/Windows.Devices.Bluetooth.0.h"
#include "internal/platform/implementation/windows/generated/winrt/impl/Windows.Devices.Bluetooth.GenericAttributeProfile.0.h"
#include "internal/platform/logging.h"

// WinRT headers
#include "internal/platform/uuid.h"
#include "winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Storage.Streams.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {

namespace {
using ::winrt::Windows::Devices::Bluetooth::BluetoothLEDevice;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristic;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicProperties;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicsResult;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattClientCharacteristicConfigurationDescriptorValue;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDescriptor;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDescriptorsResult;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceServicesResult;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattOpenStatus;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSharingMode;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattValueChangedEventArgs;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteOption;
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteResult;
using ::winrt::Windows::Foundation::IAsyncOperation;
using ::winrt::Windows::Foundation::IReference;
using ::winrt::Windows::Foundation::Collections::IVectorView;
using ::winrt::Windows::Storage::Streams::DataReader;
using ::winrt::Windows::Storage::Streams::DataWriter;
using ::winrt::Windows::Storage::Streams::IBuffer;

std::string GattCommunicationStatusToString(GattCommunicationStatus status) {
  switch (status) {
    case GattCommunicationStatus::Success:
      return "Success";
    case GattCommunicationStatus::Unreachable:
      return "Unreachable";
    case GattCommunicationStatus::ProtocolError:
      return "ProtocolError";
    case GattCommunicationStatus::AccessDenied:
      return "AccessDenied";
    default:
      return "Unknown";
  }
}

std::string GattOpenStatusToString(GattOpenStatus status) {
  switch (status) {
    case GattOpenStatus::Unspecified:
      return "Unspecified";
    case GattOpenStatus::Success:
      return "Success";
    case GattOpenStatus::AlreadyOpened:
      return "AlreadyOpened";
    case GattOpenStatus::NotFound:
      return "NotFound";
    case GattOpenStatus::SharingViolation:
      return "SharingViolation";
    case GattOpenStatus::AccessDenied:
      return "AccessDenied";
    default:
      return "Unknown";
  }
}

template <typename IGattResult>
bool CheckCommunicationStatus(IGattResult& gatt_result,
                              bool allow_access_denied = false) {
  if (!gatt_result) {
    NEARBY_LOGS(VERBOSE) << "Getting GATT Results failed.";
    return false;
  }
  GattCommunicationStatus status = gatt_result.Status();
  NEARBY_LOGS(VERBOSE) << "GattCommunicationStatus: "
                       << GattCommunicationStatusToString(status);

  return status == GattCommunicationStatus::Success ||
         (allow_access_denied &&
          status == GattCommunicationStatus::AccessDenied);
}

template <typename T, typename I>
void GetAsVector(IVectorView<T>& view, std::vector<I>& vector) {
  unsigned size = view.Size();
  vector.reserve(size);
  for (unsigned i = 0; i < size; ++i) {
    I entry = view.GetAt(i);
    vector.push_back(std::move(entry));
  }
}
}  // namespace

BleGattClient::BleGattClient(BluetoothLEDevice& ble_device)
    : ble_device_(ble_device) {}

void BleGattClient::StartGattDiscovery() {
  // Async Operation to get GattDeviceServices For Uuid
  IAsyncOperation<GattDeviceServicesResult> get_gatt_services_op =
      ble_device_.GetGattServicesAsync();
  // Result for getting GattDeviceServices
  GattDeviceServicesResult get_services_result = get_gatt_services_op.get();
  OnGetGattServices(get_services_result);
  service_discovered_ = true;
}

void BleGattClient::OnGetGattServices(
    GattDeviceServicesResult& services_result) {
  if (!CheckCommunicationStatus(services_result)) {
    NEARBY_LOGS(VERBOSE) << "Failed to get GATT services.";
    return;
  }
  NEARBY_LOGS(VERBOSE) << "Successfully get GATT services.";
  IVectorView<GattDeviceService> services = services_result.Services();
  GetAsVector(services, gatt_services_);
  for (auto& gatt_service : gatt_services_) {
    // Async Operation to open GattDeviceServices
    IAsyncOperation<GattOpenStatus> open_service_op =
        gatt_service.OpenAsync(GattSharingMode::SharedReadAndWrite);
    GattOpenStatus open_status = open_service_op.get();
    OnServiceOpen(gatt_service, gatt_service.AttributeHandle(), open_status);
  }
}

void BleGattClient::OnServiceOpen(GattDeviceService& gatt_service,
                                  uint16_t service_attribute_handle,
                                  GattOpenStatus open_status) {
  if (open_status != GattOpenStatus::Success &&
      open_status != GattOpenStatus::AlreadyOpened) {
    NEARBY_LOGS(VERBOSE) << "Ignoring failure to open service: "
                         << service_attribute_handle << ": "
                         << GattOpenStatusToString(open_status);
    service_to_characteristics_map_.insert({service_attribute_handle, {}});
    return;
  }
  NEARBY_LOGS(VERBOSE) << "Successfully open service: "
                       << service_attribute_handle;
  // Async Operation to get GattCharacteristics
  IAsyncOperation<GattCharacteristicsResult> get_characteristics_op =
      gatt_service.GetCharacteristicsAsync();
  // Result for getting GattCharacteristics
  GattCharacteristicsResult characteristics_result =
      get_characteristics_op.get();
  OnGetCharacteristics(service_attribute_handle, characteristics_result);
}

void BleGattClient::OnGetCharacteristics(
    uint16_t service_attribute_handle,
    GattCharacteristicsResult& characteristics_result) {
  if (!CheckCommunicationStatus(characteristics_result,
                                /*allow_access_denied=*/true)) {
    NEARBY_LOGS(VERBOSE) << "Failed to get characteristics for service:"
                         << service_attribute_handle;
    return;
  }
  NEARBY_LOGS(VERBOSE) << "Successfully get characteristics for service:"
                       << service_attribute_handle;
  IVectorView<GattCharacteristic> characteristics =
      characteristics_result.Characteristics();
  auto& characteristics_list =
      service_to_characteristics_map_[service_attribute_handle];
  GetAsVector(characteristics, characteristics_list);
}

BleGattClient::GattCharacteristicList& BleGattClient::GetCharacteristics(
    std::string_view service_uuid) {
  if (!service_discovered_) {
    StartGattDiscovery();
  }
  uint16_t service_attribute_handle;
  winrt::guid service_guid(service_uuid);
  for (auto& gatt_service : gatt_services_) {
    if (gatt_service.Uuid() == service_guid) {
      service_attribute_handle = gatt_service.AttributeHandle();
      break;
    }
  }
  return service_to_characteristics_map_[service_attribute_handle];
}

absl::optional<GattCharacteristic> BleGattClient::GetCharacteristic(
    std::string_view service_uuid, std::string_view characteristic_uuid) {
  NEARBY_LOGS(VERBOSE) << "Getting characteristic: " << characteristic_uuid
                       << " on service: " << service_uuid;
  winrt::guid characteristic_guid(characteristic_uuid);
  GattCharacteristicList characteristics = GetCharacteristics(service_uuid);
  for (auto& characteristic : characteristics) {
    if (characteristic.Uuid() == characteristic_guid) {
      NEARBY_LOGS(VERBOSE) << "Characteristic " << characteristic_uuid
                           << " found.";
      return characteristic;
    }
  }
  return absl::nullopt;
}

bool BleGattClient::WriteCharacteristic(std::string_view service_uuid,
                                        std::string_view characteristic_uuid,
                                        const ByteArray& value) {
  absl::optional<GattCharacteristic> characteristic =
      GetCharacteristic(service_uuid, characteristic_uuid);
  if (characteristic.has_value()) {
    return WriteCharacteristic(characteristic.value(), value);
  }
  return false;
}

bool BleGattClient::WriteCharacteristic(GattCharacteristic& characteristics,
                                        const ByteArray& value) {
  NEARBY_LOGS(VERBOSE) << "Writting " << value.size() << " bytes "
                       << "on characteristics: "
                       << characteristics.AttributeHandle();
  DataWriter data_writer;
  for (int i = 0; i < value.size(); ++i) {
    data_writer.WriteByte(static_cast<uint8_t>(*(value.data() + i)));
  }
  IBuffer buffer = data_writer.DetachBuffer();
  SubscribeToNotifications(characteristics);
  IAsyncOperation<GattWriteResult> write_value_op =
      characteristics.WriteValueWithResultAsync(
          buffer, GattWriteOption::WriteWithResponse);
  GattWriteResult result = write_value_op.get();
  return OnWriteValueWithResultAndOption(result);
}

bool BleGattClient::OnWriteValueWithResultAndOption(
    GattWriteResult& write_result) {
  GattCommunicationStatus status = write_result.Status();
  if (status == GattCommunicationStatus::Success) {
    NEARBY_LOGS(VERBOSE) << "Successfully write characteristic.";
    return true;
  }
  NEARBY_LOGS(VERBOSE) << "Error write characteristic with unexpected status: "
                       << GattCommunicationStatusToString(status);
  return false;
}

void BleGattClient::SubscribeToNotifications(
    GattCharacteristic& characteristics) {
  if (WriteCccDescriptor(
          characteristics,
          GetCccdValue(characteristics.CharacteristicProperties()))) {
    AddValueChangedHandler(characteristics);
    NEARBY_LOGS(VERBOSE) << "Successfully subscribe for value changes.";
  } else {
    NEARBY_LOGS(VERBOSE) << "Error registering for value changes.";
  }
}

void BleGattClient::UnsubscribeFromNotifications(
    GattCharacteristic& characteristics) {
  if (WriteCccDescriptor(
          characteristics,
          GattClientCharacteristicConfigurationDescriptorValue::None)) {
    RemoveValueChangedHandler(characteristics);
    NEARBY_LOGS(VERBOSE) << "Successfully un-registered for notifications.";
  } else {
    NEARBY_LOGS(VERBOSE) << "Error un-registered for notifications.";
  }
}

bool BleGattClient::WriteCccDescriptor(
    GattCharacteristic& characteristics,
    GattClientCharacteristicConfigurationDescriptorValue value) {
  try {
    IAsyncOperation<GattCommunicationStatus> write_cccdValue_op =
        characteristics.WriteClientCharacteristicConfigurationDescriptorAsync(
            value);
    GattCommunicationStatus status = write_cccdValue_op.get();
    if (status == GattCommunicationStatus::Success) {
      NEARBY_LOGS(VERBOSE) << "Successfully write client characteristic "
                              "configuration descriptor";
      return true;
    } else {
      NEARBY_LOGS(VERBOSE) << "Error write client characteristic configuration "
                              "descriptor: Status = "
                           << GattCommunicationStatusToString(status);
      return false;
    }
  } catch (std::exception exception) {
    // This usually happens when a device reports that it support notify, but
    // it actually doesn't.
    NEARBY_LOGS(ERROR) << __func__
                       << ": Exception to write client characteristic "
                          "configuration descriptor";
  }
  return false;
}

GattClientCharacteristicConfigurationDescriptorValue
BleGattClient::GetCccdValue(GattCharacteristicProperties properties) {
  GattClientCharacteristicConfigurationDescriptorValue cccdValue =
      GattClientCharacteristicConfigurationDescriptorValue::None;
  if ((properties & GattCharacteristicProperties::Indicate) !=
      GattCharacteristicProperties::None) {
    cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Indicate;
  } else if ((properties & GattCharacteristicProperties::Notify) !=
             GattCharacteristicProperties::None) {
    cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Notify;
  }
  return cccdValue;
}

void BleGattClient::AddValueChangedHandler(
    GattCharacteristic& characteristics) {
  if (!notifications_token_) {
    NEARBY_LOGS(VERBOSE)
        << " notifications_token_ = characteristics.ValueChanged";
    notifications_token_ =
        characteristics.ValueChanged({this, &BleGattClient::OnValueChanged});
  }
}
void BleGattClient::RemoveValueChangedHandler(
    GattCharacteristic& characteristics) {
  if (notifications_token_) {
    characteristics.ValueChanged(std::exchange(notifications_token_, {}));
  }
}
void BleGattClient::OnValueChanged(GattCharacteristic const& characteristic,
                                   GattValueChangedEventArgs args) {
  IBuffer value = args.CharacteristicValue();
  NEARBY_LOGS(VERBOSE) << "onValueChange:";
}

}  // namespace windows
}  // namespace nearby
