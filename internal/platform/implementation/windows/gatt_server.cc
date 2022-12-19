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
#include "internal/platform/implementation/windows/gatt_server.h"

#include <exception>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/escaping.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/gatt_characteristic.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.Advertisement.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Storage.Streams.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/logging.h"

namespace location {
namespace nearby {
namespace windows {

// The following should be centralized, to be shared across layers
// The most significant bits and the least significant bits for the Copresence
// UUID.
const std::uint64_t kCopresenceServiceUuidMsb = 0x0000FEF300001000;
const std::uint64_t kCopresenceServiceUuidLsb = 0x800000805F9B34FB;

ABSL_CONST_INIT const Uuid kCopresenceServiceUuid(kCopresenceServiceUuidMsb,
                                                  kCopresenceServiceUuidLsb);

GattServer::GattServer(
    location::nearby::api::ble_v2::ServerGattConnectionCallback server_callback)
    : server_callback_(server_callback) {
  winrt::guid rtGuid = winrt::guid(std::string(kCopresenceServiceUuid));
  winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattServiceProviderResult result =
          winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattServiceProvider::CreateAsync(rtGuid)
                  .get();

  if (result.Error() ==
      winrt::Windows::Devices::Bluetooth::BluetoothError::Success) {
    service_provider_ = result.ServiceProvider();
  }
  NEARBY_LOGS(INFO) << "Created GattServer";
}

void GattServer::StartAdvertising() {
  service_provider_.AdvertisementStatusChanged(
      {[this](winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
                  GattServiceProvider service_provider,
              winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
                  GattServiceProviderAdvertisementStatusChangedEventArgs
                      event_args) {
        auto status = event_args.Status();
        std::string prefix = "GattServer advertisement status changed: ";
        switch (status) {
          case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattServiceProviderAdvertisementStatus::Aborted:
            NEARBY_LOGS(INFO) << prefix << "Aborted";
            break;
          case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattServiceProviderAdvertisementStatus::Created:
            NEARBY_LOGS(INFO) << prefix << "Created";
            break;
          case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattServiceProviderAdvertisementStatus::Started:
            NEARBY_LOGS(INFO) << prefix << "Started";
            break;
          case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattServiceProviderAdvertisementStatus::
                  StartedWithoutAllAdvertisementData:
            NEARBY_LOGS(INFO) << prefix << "StartedWithoutAllAdvertisementData";
            break;
          case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattServiceProviderAdvertisementStatus::Stopped:
            NEARBY_LOGS(INFO) << prefix << "Stopped";
            break;
          default:
            NEARBY_LOGS(INFO) << prefix << "unknown";
            break;
        }
      }});
  winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattServiceProviderAdvertisingParameters parameters;

  // TODO(jfcarroll): I don't know where to get these?
  parameters.IsConnectable(true);
  parameters.IsDiscoverable(true);

  //  // (AD type 0x16) Service Data
  // winrt::Windows::Storage::Streams::DataWriter data_writer;

  // data_writer.WriteByte(0x4A);
  // data_writer.WriteByte(0x6F);
  // data_writer.WriteByte(0x68);
  // data_writer.WriteByte(0x6E);
  // data_writer.WriteByte(0x73);
  // data_writer.WriteByte(0x42);
  // data_writer.WriteByte(0x6C);
  // data_writer.WriteByte(0x65);
  //
  // parameters.ServiceData(data_writer.DetachBuffer());

  service_provider_.StartAdvertising(parameters);
}

void GattServer::StopAdvertising() {
  service_provider_.AdvertisementStatusChanged(
      {[this](winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
                  GattServiceProvider service_provider,
              winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
                  GattServiceProviderAdvertisementStatusChangedEventArgs
                      event_args) {
        auto status = event_args.Status();
        std::string prefix = "GattServer advertisement status changed: ";
        switch (status) {
          case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattServiceProviderAdvertisementStatus::Aborted:
            NEARBY_LOGS(INFO) << prefix << "Aborted";
            break;
          case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattServiceProviderAdvertisementStatus::Created:
            NEARBY_LOGS(INFO) << prefix << "Created";
            break;
          case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattServiceProviderAdvertisementStatus::Started:
            NEARBY_LOGS(INFO) << prefix << "Started";
            break;
          case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattServiceProviderAdvertisementStatus::
                  StartedWithoutAllAdvertisementData:
            NEARBY_LOGS(INFO) << prefix << "StartedWithoutAllAdvertisementData";
            break;
          case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattServiceProviderAdvertisementStatus::Stopped:
            NEARBY_LOGS(INFO) << prefix << "Stopped";
            break;
          default:
            NEARBY_LOGS(INFO) << prefix << "unknown";
            break;
        }
      }});

  service_provider_.StopAdvertising();
}

absl::optional<api::ble_v2::GattCharacteristic>
GattServer::CreateCharacteristic(
    const Uuid& service_uuid, const Uuid& characteristic_uuid,
    const std::vector<
        location::nearby::api::ble_v2::GattCharacteristic::Permission>&
        permissions,
    const std::vector<
        location::nearby::api::ble_v2::GattCharacteristic::Property>&
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
    const location::nearby::ByteArray& value) {
  NEARBY_LOGS(INFO)
      << "Windows Ble GattServer UpdateCharacteristic, characteristic=("
      << characteristic.service_uuid.Get16BitAsString() << ","
      << std::string(characteristic.uuid)
      << "), value = " << absl::BytesToHexString(value.data());
  // TODO(jfcarroll): The following is not valid here, what to do instead?
  // MediumEnvironment::Instance().InsertBleV2MediumGattCharacteristics(
  //    characteristic, value);

  return true;
}

// Stops a GATT server.
void GattServer::Stop() {}

}  // namespace windows
}  // namespace nearby
}  // namespace location
