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

#include "internal/platform/implementation/windows/bluetooth_classic_medium.h"

#include <synchapi.h>
#include <windows.h>

#include <string>

#include "absl/strings/str_format.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_classic_device.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.Rfcomm.h"

#include "gtest/gtest.h"

// TODO(jfcarroll): Find a way to mock winrt components in order to properly
// unit test this. Once that's done, unit tests can be written, in a later C/L.
//
// CAUTION: THIS IS NOT A REAL TEST, THIS EXERCISES THE SCANNER, IT MAY NOT
// STOP/HANG AND IS INTENDED SOLELY FOR DEBUG AND DEMONSTRATION PURPOSES. DO NOT
// ATTEMPT TO BUILD AND RUN THIS TEST ON GOOGLE3 YOU HAVE BEEN WARNED

using ::nearby::windows::BluetoothDevice;

typedef std::map<const std::string, nearby::api::BluetoothDevice*> DeviceMap;

class BluetoothClassicMediumTests : public testing::Test {
 protected:
  static void device_discovered_cb(nearby::api::BluetoothDevice& device) {
    const std::string address = device.GetMacAddress();

    std::map<std::string, nearby::api::BluetoothDevice*>::const_iterator it =
        deviceList.find(address);

    std::string buffer =
        absl::StrFormat("processing device %s : ", address.c_str());

    OutputDebugStringA(buffer.c_str());

    if (it == deviceList.end()) {
      buffer = absl::StrFormat("adding device %s\n", address.c_str());

      OutputDebugStringA(buffer.c_str());
      deviceList[device.GetMacAddress()] = &device;
    }
  }

  static void device_name_changed_cb(nearby::api::BluetoothDevice& device) {}

  static void device_lost_cb(nearby::api::BluetoothDevice& device) {
    deviceList.erase(device.GetMacAddress());
  }

  int callcount = 0;
  static inline DeviceMap deviceList = {};
};

#ifdef TESTING_LOCALLY
TEST_F(BluetoothClassicMediumTests, ManualTest) {
  auto bluetoothAdapter = nearby::windows::BluetoothAdapter();

  std::string bluetoothAdapterName = bluetoothAdapter.GetName();
  bluetoothAdapter.SetName("BluetoothTestName");
  bluetoothAdapterName = bluetoothAdapter.GetName();
  bluetoothAdapter.SetName("");
  bluetoothAdapterName = bluetoothAdapter.GetName();

  std::unique_ptr<nearby::windows::BluetoothClassicMedium> bcm =
      std::make_unique<nearby::windows::BluetoothClassicMedium>(
          bluetoothAdapter);

  bluetoothAdapter.SetScanMode(
      nearby::windows::BluetoothAdapter::ScanMode::kConnectableDiscoverable);

  auto mode = bluetoothAdapter.GetScanMode();

  bcm->StartDiscovery(nearby::api::BluetoothClassicMedium::DiscoveryCallback{
      .device_discovered_cb = device_discovered_cb,
      .device_name_changed_cb = device_name_changed_cb,
      .device_lost_cb = device_lost_cb,
  });

  Sleep(30000);

  auto currentDevice = deviceList["DC:DC:E2:F2:B5:99"];
  auto serviceUuid = winrt::to_string(
      winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceId::SerialPort()
          .AsString());

  nearby::CancellationFlag cancellationFlag;
  bcm->ConnectToService(*currentDevice,
                        //  "a82efa21-ae5c-3dde-9bbc-f16da7b16c5a",
                        "00001101-0000-1000-8000-00805F9B34FB",
                        &cancellationFlag);

  EXPECT_EQ(1, 1);
  EXPECT_TRUE(true);
}
#endif
TEST_F(BluetoothClassicMediumTests, ConnectToServiceNullUuid) {
  // Arrange
  auto bluetoothAdapter = nearby::windows::BluetoothAdapter();

  std::unique_ptr<nearby::windows::BluetoothClassicMedium>
      bluetoothClassicMedium =
          std::make_unique<nearby::windows::BluetoothClassicMedium>(
              bluetoothAdapter);

  nearby::windows::BluetoothDevice bluetoothDevice =
      nearby::windows::BluetoothDevice(std::string("1D:EA:DB:EE:B9:00"));
  nearby::CancellationFlag cancellationFlag;

  auto bluetoothClassicMediumImpl = bluetoothClassicMedium.get();

  // Act
  auto asyncResult = bluetoothClassicMediumImpl->ConnectToService(
      bluetoothDevice, std::string(), &cancellationFlag);

  // Assert
  EXPECT_TRUE(asyncResult.get() == nullptr);
}

TEST_F(BluetoothClassicMediumTests, ConnectToServiceNullCancellationFlag) {
  // Arrange
  auto bluetoothAdapter = nearby::windows::BluetoothAdapter();

  std::unique_ptr<nearby::windows::BluetoothClassicMedium>
      bluetoothClassicMedium =
          std::make_unique<nearby::windows::BluetoothClassicMedium>(
              bluetoothAdapter);

  nearby::windows::BluetoothDevice bluetoothDevice =
      nearby::windows::BluetoothDevice(std::string("1D:EA:DB:EE:B9:00"));

  auto bluetoothClassicMediumImpl = bluetoothClassicMedium.get();

  // Act
  auto asyncResult = bluetoothClassicMediumImpl->ConnectToService(
      bluetoothDevice, std::string("test service"), nullptr);

  // Assert
  EXPECT_TRUE(asyncResult.get() == nullptr);
}

TEST_F(BluetoothClassicMediumTests, ConnectToServiceWithInvalidServiceUuid) {
  // Arrange
  auto bluetoothAdapter = nearby::windows::BluetoothAdapter();

  std::unique_ptr<nearby::windows::BluetoothClassicMedium>
      bluetoothClassicMedium =
          std::make_unique<nearby::windows::BluetoothClassicMedium>(
              bluetoothAdapter);

  nearby::windows::BluetoothDevice bluetoothDevice =
      nearby::windows::BluetoothDevice(std::string("1D:EA:DB:EE:B9:00"));
  nearby::CancellationFlag cancellationFlag;

  auto bluetoothClassicMediumImpl = bluetoothClassicMedium.get();

  // Act
  auto asyncResult = bluetoothClassicMediumImpl->ConnectToService(
      bluetoothDevice, std::string("test service"), &cancellationFlag);

  // Assert
  EXPECT_TRUE(asyncResult.get() == nullptr);
}
