// Copyright 2020 Google LLC
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

#include "platform/impl/windows/bluetooth_classic_medium.h"

#include <Windows.h>
#include <synchapi.h>

#include <string>

#include "platform/impl/windows/generated/winrt/Windows.Devices.Bluetooth.Rfcomm.h"
#include "absl/strings/str_format.h"
#include "platform/impl/windows/bluetooth_classic_device.h"

#include "gtest/gtest.h"

// TODO(jfcarroll): Find a way to mock winrt components in order to properly
// unit test this. Once that's done, unit tests can be written, in a later C/L.
//
// CAUTION: THIS IS NOT A REAL TEST, THIS EXERCISES THE SCANNER, IT DOES NOT
// STOP AND IS INTENDED SOLELY FOR DEBUG AND DEMONSTRATION PURPOSES. DO NOT
// ATTEMPT TO BUILD AND RUN THIS TEST ON GOOGLE3 YOU HAVE BEEN WARNED
static int callcount = 0;
static std::map<std::string, location::nearby::api::BluetoothDevice*>
    deviceList;

void device_discovered_cb(location::nearby::api::BluetoothDevice& device) {
  const std::string address = device.GetMacAddress();

  std::map<std::string, location::nearby::api::BluetoothDevice*>::const_iterator
      it = deviceList.find(address);

  std::string buffer =
      absl::StrFormat("processing device %s : ", address.c_str());

  OutputDebugStringA(buffer.c_str());

  if (it == deviceList.end()) {
    buffer = absl::StrFormat("adding device %s\n", address.c_str());

    OutputDebugStringA(buffer.c_str());
    deviceList[device.GetMacAddress()] = &device;
  }
}

void device_name_changed_cb(location::nearby::api::BluetoothDevice& device) {}

void device_lost_cb(location::nearby::api::BluetoothDevice& device) {
  deviceList.erase(device.GetMacAddress());
}

TEST(TestCaseName, TestName) {
  std::unique_ptr<location::nearby::windows::BluetoothClassicMedium> bcm =
      std::make_unique<location::nearby::windows::BluetoothClassicMedium>();

  bcm->StartDiscovery(
      location::nearby::api::BluetoothClassicMedium::DiscoveryCallback{
          .device_discovered_cb = device_discovered_cb,
          .device_name_changed_cb = device_name_changed_cb,
          .device_lost_cb = device_lost_cb,
      });

  Sleep(30000);

  auto currentDevice = deviceList["DC:DC:E2:F2:B5:99"];
  auto serviceUuid = winrt::Windows::Devices::Bluetooth::Rfcomm::
      RfcommServiceId::GenericFileTransfer();
  location::nearby::CancellationFlag cancellationFlag;
  bcm->ConnectToService(*currentDevice, "a82efa21-ae5c-3dde-9bbc-f16da7b16c5a",
                        &cancellationFlag);

  EXPECT_EQ(1, 1);
  EXPECT_TRUE(true);
}
