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
// See the License for the specific language governing desired_permissions and
// limitations under the License.
#include "internal/platform/implementation/windows/ble_gatt_characteristic.h"

#include <unknwn.h>

#include <string>
#include <vector>

#include "internal/platform/implementation/windows/ble_gatt_server.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Storage.Streams.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace nearby {
namespace windows {

// Specifies the values for the GATT characteristic properties as well as the
// GATT Extended Characteristic Properties Descriptor. Provides a collection
// of flags representing the GATT Characteristic Properties and if the GATT
// Extended Properties Descriptor is present the GATT Extended Characteristic
// properties of the characteristic. Represents the GATT characteristic
// properties, as defined by the GATT profile, and if the ExtendedProperties
// flag is present it also represents the properties of the Extended
// Characteristic Properties Descriptor This enumeration supports a bitwise
// combination of its member values.
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattcharacteristicproperties?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicProperties;

// This class contains the local characteristic descriptor parameters.
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattlocalcharacteristicparameters?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattLocalCharacteristicParameters;

// This class represents a Bluetooth GATT read request.
// https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattreadrequest?view=winrt-22621
using ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattReadRequest;

// Specifies the byte order of a stream.
// https://learn.microsoft.com/en-us/uwp/api/windows.storage.streams.byteorder?view=winrt-22621
using ::winrt::Windows::Storage::Streams::ByteOrder;

// Writes data to an output stream.
// https://learn.microsoft.com/en-us/uwp/api/windows.storage.streams.datawriter?view=winrt-22621
using ::winrt::Windows::Storage::Streams::DataWriter;

winrt::fire_and_forget GattCharacteristic::HandleReadEvent(
    GattLocalCharacteristic const&, GattReadRequestedEventArgs args) {
  // BT_Code: Process a read request.
  auto deferral = args.GetDeferral();

  // Get the request information.  This requires device access before an app can
  // access the device's request.
  GattReadRequest request = args.GetRequestAsync().get();
  if (request == nullptr) {
    // No access allowed to the device.  Application should indicate this to the
    // user.
    printf("Access to device not allowed");
  } else {
    DataWriter writer;
    writer.ByteOrder(ByteOrder::LittleEndian);

    for (auto current_byte : current_value_.AsStringView()) {
      writer.WriteByte(current_byte);
    }

    // Developer note:
    // Can get details about the request such as the size and offset, as well
    // as monitor the state to see if it has been completed/cancelled
    // externally.
    // can check:
    //    request.Offset
    //    request.Length
    //    request.State
    //
    // And add a StateChanded handler here.
    // request.StateChanged(Handler);

    // Gatt code to handle the response
    request.RespondWithValue(writer.DetachBuffer());
  }

  deferral.Complete();
  return winrt::fire_and_forget();
}

winrt::fire_and_forget GattCharacteristic::HandleWriteEvent(
    GattLocalCharacteristic const&, GattWriteRequestedEventArgs args) {
  // TODO(jfcarroll): Figure out what needs to be done here.
  return winrt::fire_and_forget();
}

winrt::fire_and_forget GattCharacteristic::HandleSubscribedClientsChangedEvent(
    GattLocalCharacteristic const&, IInspectable const& args) {
  // TODO(jfcarroll): Figure out what needs to be done here.
  return winrt::fire_and_forget();
}

GattCharacteristic::GattCharacteristic(
    GattServer& gatt_server_owner, Uuid desired_service_uuid,
    Uuid desired_characteristic_uuid,
    const std::vector<api::ble_v2::GattCharacteristic::Permission>&
        desired_permissions,
    const std::vector<api::ble_v2::GattCharacteristic::Property>&
        desired_properties)
    : gatt_server_(gatt_server_owner) {
  uuid = desired_characteristic_uuid;
  service_uuid = desired_service_uuid;
  permissions = desired_permissions;
  properties = desired_properties;

  // This class contains the local characteristic descriptor parameters.
  // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattlocalcharacteristicparameters?view=winrt-22621
  auto parameters = GattLocalCharacteristicParameters();

  auto characteristic_properties = GattCharacteristicProperties::None;

  for (auto property : properties) {
    if (property ==
        nearby::api::ble_v2::GattCharacteristic::Property::kIndicate) {
      // https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/gatt-server
      characteristic_properties |= GattCharacteristicProperties::Indicate;
    }
    if (property == nearby::api::ble_v2::GattCharacteristic::Property::kRead) {
      characteristic_properties |= GattCharacteristicProperties::Read;
    }
    if (property == nearby::api::ble_v2::GattCharacteristic::Property::kWrite) {
      characteristic_properties |= GattCharacteristicProperties::Write;
    }
  }
  parameters.CharacteristicProperties(characteristic_properties);

  winrt::guid myGuid{std::string(uuid)};

  auto characteristicResult = gatt_server_.GetServiceProvider()
                                  .Service()
                                  .CreateCharacteristicAsync(myGuid, parameters)
                                  .get();

  gatt_characteristic_ = characteristicResult.Characteristic();

  subscribed_clients_ = gatt_characteristic_.SubscribedClients();

  gatt_characteristic_.ReadRequested(
      {this, &GattCharacteristic::HandleReadEvent});
  gatt_characteristic_.WriteRequested(
      {this, &GattCharacteristic::HandleWriteEvent});
  gatt_characteristic_.SubscribedClientsChanged(
      {this, &GattCharacteristic::HandleSubscribedClientsChangedEvent});
}

}  // namespace windows
}  // namespace nearby
