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

#include "internal/platform/bluetooth_utils.h"

#include "gtest/gtest.h"

namespace nearby {

constexpr absl::string_view kBluetoothMacAddress{"00:00:E6:88:64:13"};
constexpr char kBluetoothMacAddressBytes[] = {0x00, 0x00, 0xe6,
                                              0x88, 0x64, 0x13};

TEST(BluetoothUtilsTest, ToStringWorks) {
  ByteArray bt_mac_address_bytes{kBluetoothMacAddressBytes,
                                 sizeof(kBluetoothMacAddressBytes)};

  auto bt_mac_address = BluetoothUtils::ToString(bt_mac_address_bytes);

  EXPECT_EQ(kBluetoothMacAddress, bt_mac_address);
}

TEST(BluetoothUtilsTest, FromStringWorks) {
  ByteArray bt_mac_address_bytes{kBluetoothMacAddressBytes,
                                 sizeof(kBluetoothMacAddressBytes)};

  auto bt_mac_address_bytes_result =
      BluetoothUtils::FromString(kBluetoothMacAddress);

  EXPECT_EQ(bt_mac_address_bytes, bt_mac_address_bytes_result);
}

TEST(BluetoothUtilsTest, InvalidBytesReturnsEmptyString) {
  std::string string_result;

  char bad_bt_mac_address_1[] = {0x02, 0x20, 0x00};
  ByteArray bad_bt_mac_address_bytes_1{bad_bt_mac_address_1,
                                       sizeof(bad_bt_mac_address_1)};
  string_result = BluetoothUtils::ToString(bad_bt_mac_address_bytes_1);
  EXPECT_TRUE(string_result.empty());

  char bad_bt_mac_address_2[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  ByteArray bad_bt_mac_address_bytes_2{bad_bt_mac_address_2,
                                       sizeof(bad_bt_mac_address_2)};
  string_result = BluetoothUtils::ToString(bad_bt_mac_address_bytes_2);
  EXPECT_TRUE(string_result.empty());

  char bad_bt_mac_address_3[] = {0x11, 0x22, 0x33, 0x44, 0x55,
                                 0x66, 0x77, 0x88, 0x99};
  ByteArray bad_bt_mac_address_bytes_3{bad_bt_mac_address_3,
                                       sizeof(bad_bt_mac_address_3)};
  string_result = BluetoothUtils::ToString(bad_bt_mac_address_bytes_3);
  EXPECT_TRUE(string_result.empty());
}

TEST(BluetoothUtilsTest, InvalidStringReturnsEmptyByteArray) {
  ByteArray bytes_result;

  std::string bad_bt_mac_address_1 = "022:00";
  bytes_result = BluetoothUtils::FromString(bad_bt_mac_address_1);
  EXPECT_TRUE(bytes_result.Empty());

  std::string bad_bt_mac_address_2 = "22:00:11:33:77:aa::bb::99";
  bytes_result = BluetoothUtils::FromString(bad_bt_mac_address_2);
  EXPECT_TRUE(bytes_result.Empty());

  std::string bad_bt_mac_address_3 = "00:00:00:00:00:00";
  bytes_result = BluetoothUtils::FromString(bad_bt_mac_address_3);
  EXPECT_TRUE(bytes_result.Empty());

  std::string bad_bt_mac_address_4 = "BLUETOOTHCHIP";
  bytes_result = BluetoothUtils::FromString(bad_bt_mac_address_4);
  EXPECT_TRUE(bytes_result.Empty());
}

}  // namespace nearby
