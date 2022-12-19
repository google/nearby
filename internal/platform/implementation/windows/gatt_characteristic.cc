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
#include "internal/platform/implementation/windows/gatt_characteristic.h"

#include <unknwn.h>

#include <string>
#include <vector>

#include "internal/platform/implementation/windows/gatt_server.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Storage.Streams.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "winrt/Windows.ApplicationModel.Activation.h"
#include "winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h"
#include "winrt/Windows.Devices.Bluetooth.h"
#include "winrt/Windows.Devices.Enumeration.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Security.Cryptography.h"
#include "winrt/Windows.Storage.Streams.h"
#include "winrt/Windows.System.h"

namespace location {
namespace nearby {
namespace windows {

winrt::fire_and_forget GattCharacteristic::HandleReadEvent(
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattLocalCharacteristic const&,
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattReadRequestedEventArgs args) {
  // Get the request information.  This requires device access before an app can
  // access the device's request.
  winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadRequest
      request = args.GetRequestAsync().get();
  if (request == nullptr) {
    // No access allowed to the device.  Application should indicate this to the
    // user.
    printf("Access to device not allowed");
  } else {
    // BT_Code: Process a read request.
    // auto lifetime = winrt::get_strong();
    auto deferral = args.GetDeferral();

    // Get the request information.  This requires device access before an app
    // can access the device's request.
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadRequest
        request = args.GetRequestAsync().get();
    if (request == nullptr) {
      // No access allowed to the device.  Application should indicate this to
      // the user.
      // rootPage.NotifyUser(L"Access to device not allowed",
      //                    NotifyType::ErrorMessage);
    } else {
      winrt::Windows::Storage::Streams::DataWriter writer;
      writer.ByteOrder(
          winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);

      for (auto current_byte : current_value_.AsStringView()) {
        writer.WriteByte(current_byte);
      }
      // Can get details about the request such as the size and offset, as well
      // as monitor the state to see if it has been completed/cancelled
      // externally. request.Offset request.Length request.State
      // request.StateChanged += <Handler>

      // Gatt code to handle the response
      request.RespondWithValue(writer.DetachBuffer());
    }

    deferral.Complete();
  }

  return winrt::fire_and_forget();
}

winrt::fire_and_forget GattCharacteristic::HandleWriteEvent(
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattLocalCharacteristic const&,
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattWriteRequestedEventArgs args) {
  return winrt::fire_and_forget();
}

winrt::fire_and_forget GattCharacteristic::HandleSubscribedClientsChangedEvent(
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
        GattLocalCharacteristic const& gatt_local_characteristic,
    winrt::Windows::Foundation::IInspectable const& args) {
  auto updated_list = gatt_local_characteristic.SubscribedClients();

  // Iterate through that collection.For each element, do the following :
  //      Access the GattSubscribedClient.Session property, which is a
  //             GattSession object.
  //      Access the GattSession.DeviceId property, which is a
  //             BluetoothDeviceId object.
  //      Access the BluetoothDeviceId.Id property, which is the device ID
  //        string.
  //      Pass the device ID string to BluetoothLEDevice.FromIdAsync
  //           to retrieve a BluetoothLEDevice
  //           object.
  //               You can get full information about the device from that
  //               object.

  for (auto subscribed_client : subscribed_clients_) {
  }

  return winrt::fire_and_forget();
}

GattCharacteristic::GattCharacteristic(
    location::nearby::windows::GattServer& gatt_server,
    Uuid desired_service_uuid, Uuid characteristic_uuid,
    const std::vector<
        location::nearby::api::ble_v2::GattCharacteristic::Permission>&
        desired_permissions,
    const std::vector<
        location::nearby::api::ble_v2::GattCharacteristic::Property>&
        desired_properties) {
  uuid = characteristic_uuid;
  service_uuid = desired_service_uuid;
  permissions = desired_permissions;
  properties = desired_properties;

  // This class contains the local characteristic descriptor parameters.
  // https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.genericattributeprofile.gattlocalcharacteristicparameters?view=winrt-22621
  winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattLocalCharacteristicParameters parameters =
          winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattLocalCharacteristicParameters();

  auto characteristic_properties = winrt::Windows::Devices::Bluetooth::
      GenericAttributeProfile::GattCharacteristicProperties::None;

  for (auto property : properties) {
    if (property == location::nearby::api::ble_v2::GattCharacteristic::
                        Property::kIndicate) {
      // Generated Attributes
      // The following descriptors are autogenerated by the system, based on the
      // GattLocalCharacteristicParameters provided during creation of the
      // characteristic:
      //
      // 1. Client Characteristic Configuration (if the characteristic is marked
      // as indicatable or notifiable).
      //
      // 2. Characteristic User Description (if UserDescription property is
      // set). See GattLocalCharacteristicParameters.UserDescription property
      // for more info.
      //
      // 3. Characteristic Format (one descriptor for each presentation
      // format specified). See
      // GattLocalCharacteristicParameters.PresentationFormats property for more
      // info.
      //
      // 4. Characteristic Aggregate Format (if more than one presentation
      // format is specified).
      //
      // 5. GattLocalCharacteristicParameters.See PresentationFormats property
      // for more info.
      //
      // 6. Characteristic Extended Properties (if the characteristic is marked
      // with the extended properties bit).
      // https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/gatt-server
      characteristic_properties |= winrt::Windows::Devices::Bluetooth::
          GenericAttributeProfile::GattCharacteristicProperties::Indicate;
    }
    if (property ==
        location::nearby::api::ble_v2::GattCharacteristic::Property::kRead) {
      characteristic_properties |= winrt::Windows::Devices::Bluetooth::
          GenericAttributeProfile::GattCharacteristicProperties::Read;
    }
    if (property ==
        location::nearby::api::ble_v2::GattCharacteristic::Property::kWrite) {
      characteristic_properties |= winrt::Windows::Devices::Bluetooth::
          GenericAttributeProfile::GattCharacteristicProperties::Write;
    }
  }
  parameters.CharacteristicProperties(characteristic_properties);

  winrt::guid myGuid{std::string(characteristic_uuid)};

  auto characteristicResult = gatt_server.GetServiceProvider()
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
}  // namespace location
