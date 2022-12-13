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

#ifndef PLATFORM_IMPL_WINDOWS_GATT_SERVER_H_
#define PLATFORM_IMPL_WINDOWS_GATT_SERVER_H_

#include <guiddef.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"

namespace location {
namespace nearby {
namespace windows {

// Provides the operations that can be performed on the Gatt Server.
class GattServer : public nearby::api::ble_v2::GattServer {
 public:
  explicit GattServer(
      location::nearby::api::ble_v2::ServerGattConnectionCallback
          server_callback);

  // Creates a characteristic
  // and adds it to the GATT
  // server under the given
  // characteristic and service UUIDs. Returns no value upon error.
  //
  // Characteristics of the same service UUID should be put under one
  // service rather than many services with the same UUID.
  //
  // If the INDICATE property is included, the characteristic should include
  // the official Bluetooth Client Characteristic Configuration descriptor
  // with UUID 0x2902 and a WRITE permission. This allows remote clients to
  // write to this descriptor and subscribe for characteristic changes. For
  // more information about this descriptor, please go to:
  // https://www.bluetooth.com/specifications/Gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.Gatt.client_characteristic_configuration.xml
  // NOLINTNEXTLINE(google3-legacy-absl-backports)
  absl::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
      const Uuid& service_uuid, const Uuid& characteristic_uuid,
      const std::vector<
          location::nearby::api::ble_v2::GattCharacteristic::Permission>&
          permissions,
      const std::vector<
          location::nearby::api::ble_v2::GattCharacteristic::Property>&
          properties) override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html#setValue(byte[])
  //
  // Locally updates the value of a characteristic and returns whether or not
  // it was successful.
  bool UpdateCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      const location::nearby::ByteArray& value) override;

  // Stops a GATT server.
  void Stop() override;

  winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
      IGattServiceProvider
      GetServiceProvider() {
    return service_provider_;
  }

 private:
  winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattServiceProvider service_provider_ = nullptr;

  location::nearby::api::ble_v2::ServerGattConnectionCallback server_callback_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif
