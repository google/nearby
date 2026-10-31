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

#include <map>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/str_format.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/windows/bluetooth_adapter.h"
#include "internal/platform/implementation/windows/bluetooth_classic_device.h"
#include "internal/platform/mac_address.h"

namespace nearby::windows {
namespace {

using DeviceMap = std::map<const std::string, api::BluetoothDevice*>;

class BluetoothClassicMediumTests : public testing::Test {
 protected:
  static void device_discovered_cb(api::BluetoothDevice& device) {
    const std::string address = device.GetMacAddress().ToString();

    auto it = deviceList.find(address);

    std::string buffer =
        absl::StrFormat("processing device %s : ", address.c_str());

    OutputDebugStringA(buffer.c_str());

    if (it == deviceList.end()) {
      buffer = absl::StrFormat("adding device %s\n", address.c_str());

      OutputDebugStringA(buffer.c_str());
      deviceList[address] = &device;
    }
  }

  static void device_name_changed_cb(api::BluetoothDevice& device) {}

  static void device_lost_cb(api::BluetoothDevice& device) {
    deviceList.erase(device.GetMacAddress().ToString());
  }

  int callcount = 0;
  static inline DeviceMap deviceList = {};
};

#ifdef TESTING_LOCALLY
TEST_F(BluetoothClassicMediumTests, ManualTest) {
  BluetoothAdapter bluetoothAdapter;

  std::string bluetoothAdapterName = bluetoothAdapter.GetName();
  bluetoothAdapter.SetName("BluetoothTestName");
  bluetoothAdapterName = bluetoothAdapter.GetName();
  bluetoothAdapter.SetName("");
  bluetoothAdapterName = bluetoothAdapter.GetName();

  auto bcm = std::make_unique<BluetoothClassicMedium>(bluetoothAdapter);

  bluetoothAdapter.SetScanMode(
      BluetoothAdapter::ScanMode::kConnectableDiscoverable);

  auto mode = bluetoothAdapter.GetScanMode();

  bcm->StartDiscovery(api::BluetoothClassicMedium::DiscoveryCallback{
      .device_discovered_cb = device_discovered_cb,
      .device_name_changed_cb = device_name_changed_cb,
      .device_lost_cb = device_lost_cb,
  });

  Sleep(30000);

  auto currentDevice = deviceList["DC:DC:E2:F2:B5:99"];
  auto serviceUuid = winrt::to_string(
      winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceId::SerialPort()
          .AsString());

  CancellationFlag cancellationFlag;
  bcm->ConnectToService(*currentDevice, "00001101-0000-1000-8000-00805F9B34FB",
                        &cancellationFlag);

  EXPECT_EQ(1, 1);
  EXPECT_TRUE(true);
}
#endif

TEST_F(BluetoothClassicMediumTests, ConnectToServiceNullUuid) {
  BluetoothAdapter bluetoothAdapter;
  BluetoothClassicMedium bluetoothClassicMedium(bluetoothAdapter);
  MacAddress mac_address;
  ASSERT_TRUE(MacAddress::FromString("1D:EA:DB:EE:B9:00", mac_address));
  BluetoothDevice bluetoothDevice("device_id", "device_name", mac_address);
  CancellationFlag cancellationFlag;

  auto asyncResult = bluetoothClassicMedium.ConnectToService(
      bluetoothDevice, std::string(), &cancellationFlag);

  EXPECT_EQ(asyncResult, nullptr);
}

TEST_F(BluetoothClassicMediumTests, ConnectToServiceNullCancellationFlag) {
  BluetoothAdapter bluetoothAdapter;
  BluetoothClassicMedium bluetoothClassicMedium(bluetoothAdapter);
  MacAddress mac_address;
  ASSERT_TRUE(MacAddress::FromString("1D:EA:DB:EE:B9:00", mac_address));
  BluetoothDevice bluetoothDevice("device_id", "device_name", mac_address);

  auto asyncResult = bluetoothClassicMedium.ConnectToService(
      bluetoothDevice, "00001101-0000-1000-8000-00805F9B34FB", nullptr);

  EXPECT_EQ(asyncResult, nullptr);
}

TEST_F(BluetoothClassicMediumTests, ConnectToServiceWithInvalidServiceUuid) {
  BluetoothAdapter bluetoothAdapter;
  BluetoothClassicMedium bluetoothClassicMedium(bluetoothAdapter);
  MacAddress mac_address;
  ASSERT_TRUE(MacAddress::FromString("1D:EA:DB:EE:B9:00", mac_address));
  BluetoothDevice bluetoothDevice("device_id", "device_name", mac_address);
  CancellationFlag cancellationFlag;

  auto asyncResult = bluetoothClassicMedium.ConnectToService(
      bluetoothDevice, "test service", &cancellationFlag);

  EXPECT_EQ(asyncResult, nullptr);
}

TEST_F(BluetoothClassicMediumTests, ListenForServiceEmptyUuid) {
  BluetoothAdapter bluetoothAdapter;
  BluetoothClassicMedium bluetoothClassicMedium(bluetoothAdapter);

  EXPECT_EQ(bluetoothClassicMedium.ListenForService("test_service", ""),
            nullptr);
}

TEST_F(BluetoothClassicMediumTests, ListenForServiceEmptyServiceName) {
  BluetoothAdapter bluetoothAdapter;
  BluetoothClassicMedium bluetoothClassicMedium(bluetoothAdapter);

  EXPECT_EQ(bluetoothClassicMedium.ListenForService(
                "", "00001101-0000-1000-8000-00805F9B34FB"),
            nullptr);
}

}  // namespace
}  // namespace nearby::windows
