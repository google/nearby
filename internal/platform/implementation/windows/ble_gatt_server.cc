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
#include "internal/platform/implementation/windows/ble_gatt_server.h"

#include <exception>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/ble_gatt_characteristic.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.Advertisement.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Storage.Streams.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {

// Specifies common Bluetooth error cases.
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.bluetootherror?view=winrt-22621
using winrt::Windows::Devices::Bluetooth::BluetoothError;

// This class is the result of the CreateAsync operation.
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattserviceproviderresult?view=winrt-22621
using winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattServiceProviderResult;

// The following should be centralized, to be shared across layers
// The most significant bits and the least significant bits for the Copresence
// UUID.
constexpr std::uint64_t kCopresenceServiceUuidMsb = 0x0000FEF300001000;
constexpr std::uint64_t kCopresenceServiceUuidLsb = 0x800000805F9B34FB;

ABSL_CONST_INIT const Uuid kCopresenceServiceUuid(kCopresenceServiceUuidMsb,
                                                  kCopresenceServiceUuidLsb);

GattServer::GattServer(ServerGattConnectionCallback server_callback)
    : server_callback_(std::move(server_callback)) {
  winrt::guid rtGuid = winrt::guid(std::string(kCopresenceServiceUuid));

  GattServiceProviderResult result =
      GattServiceProvider::CreateAsync(rtGuid).get();

  if (result.Error() == BluetoothError::Success) {
    service_provider_ = result.ServiceProvider();
  }

  service_provider_.AdvertisementStatusChanged(
      {this, &GattServer::HandleAdvertisementStatusChanged});
}

void GattServer::StartAdvertising(
    const ::nearby::api::ble_v2::BleAdvertisementData& advertising_data,
    ::nearby::api::ble_v2::AdvertiseParameters
        requested_advertising_parameters) {
  ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattServiceProviderAdvertisingParameters advertising_parameters;

  advertising_parameters.IsConnectable(
      requested_advertising_parameters.is_connectable);
  // advertising_parameters.IsConnectable(false);
  advertising_parameters.IsDiscoverable(
      requested_advertising_parameters.is_discoverable);

  auto service_data_entry =
      advertising_data.service_data.find(kCopresenceServiceUuid);

  auto service_data = service_data_entry->second;
  auto sd_sv = service_data.AsStringView();

  std::string_view sd("123456789012345678");
  ::winrt::Windows::Storage::Streams::DataWriter data_writer;

  auto buffer = service_data.data();

  // for (auto character : sd_sv) {
  //   data_writer.WriteByte(character);
  // }
  for (auto character : sd) {
    data_writer.WriteByte(character);
  }
  // data_writer.
  advertising_parameters.ServiceData(data_writer.DetachBuffer());
  auto capacity2 = advertising_parameters.ServiceData().Capacity();
  auto length2 = advertising_parameters.ServiceData().Length();

  service_provider_.StartAdvertising(advertising_parameters);
}
void GattServer::StopAdvertising() { service_provider_.StopAdvertising(); }

void GattServer::HandleAdvertisementStatusChanged(
    GattServiceProvider service_provider,
    GattServiceProviderAdvertisementStatusChangedEventArgs const&
        service_provider_advertisement_status_changed_event_args) {
  switch (service_provider_advertisement_status_changed_event_args.Status()) {
    case ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattServiceProviderAdvertisementStatus::Aborted:
      NEARBY_LOGS(INFO) << "Advertisement status changed: Aborted.";
      break;

    case ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattServiceProviderAdvertisementStatus::Created:
      NEARBY_LOGS(INFO) << "Advertisement status changed: Created.";
      break;

    case ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattServiceProviderAdvertisementStatus::Started:
      NEARBY_LOGS(INFO) << "Advertisement status changed: Started.";
      break;

    case ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattServiceProviderAdvertisementStatus::
            StartedWithoutAllAdvertisementData:
      NEARBY_LOGS(INFO) << "Advertisement status changed: "
                           "StartedWithoutAllAdvertisementData.";
      break;

    case ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattServiceProviderAdvertisementStatus::Stopped:
      NEARBY_LOGS(INFO) << "Advertisement status changed: Stopped.";
      break;

    default:
      NEARBY_LOGS(INFO) << "Advertisement status changed: Created.";
      break;
  }
}

absl::optional<api::ble_v2::GattCharacteristic>
GattServer::CreateCharacteristic(
    const Uuid& service_uuid, const Uuid& characteristic_uuid,
    const std::vector<nearby::api::ble_v2::GattCharacteristic::Permission>&
        permissions,
    const std::vector<nearby::api::ble_v2::GattCharacteristic::Property>&
        properties) {
  return windows::GattCharacteristic(*this, service_uuid, characteristic_uuid,
                                     permissions, properties);
}

// https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
//
// Locally updates the value of a characteristic and returns whether or not
// it was successful.
bool GattServer::UpdateCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    const nearby::ByteArray& value) {
  value_ = value;
  return true;
}

// Stops a GATT server.
void GattServer::Stop() {}

}  // namespace windows
}  // namespace nearby
