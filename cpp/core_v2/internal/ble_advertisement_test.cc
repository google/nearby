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

#include "core_v2/internal/ble_advertisement.h"

#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

const BleAdvertisement::Version kVersion = BleAdvertisement::Version::kV1;
const Pcp kPcp = Pcp::kP2pCluster;
const char kServiceIDHashBytes[] = {0x0A, 0x0B, 0x0C};
const char kEndPointID[] = "AB12";
const char kEndpointName[] =
    "How much wood can a woodchuck chuck if a wood chuck would chuck wood?";
const char kBluetoothMacAddress[] = "00:00:E6:88:64:13";

TEST(BleAdvertisementTest, ConstructionWorks) {
  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto ble_advertisement =
      BleAdvertisement(kVersion, kPcp, service_id_hash, kEndPointID,
                       kEndpointName, kBluetoothMacAddress);
  auto is_valid = ble_advertisement.IsValid();

  EXPECT_TRUE(is_valid);
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndPointID, ble_advertisement.GetEndpointId());
  EXPECT_EQ(kEndpointName, ble_advertisement.GetEndpointName());
  EXPECT_EQ(kBluetoothMacAddress, ble_advertisement.GetBluetoothMacAddress());
}

TEST(BleAdvertisementTest, ConstructionWorksWithEmptyEndpointName) {
  std::string empty_endpoint_name;

  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto ble_advertisement =
      BleAdvertisement(kVersion, kPcp, service_id_hash, kEndPointID,
                       empty_endpoint_name, kBluetoothMacAddress);
  auto is_valid = ble_advertisement.IsValid();

  EXPECT_TRUE(is_valid);
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndPointID, ble_advertisement.GetEndpointId());
  EXPECT_EQ(empty_endpoint_name, ble_advertisement.GetEndpointName());
  EXPECT_EQ(kBluetoothMacAddress, ble_advertisement.GetBluetoothMacAddress());
}

TEST(BleAdvertisementTest, ConstructionWorksWithEmojiEndpointName) {
  std::string emoji_endpoint_name("\u0001F450 \u0001F450");

  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto ble_advertisement =
      BleAdvertisement(kVersion, kPcp, service_id_hash, kEndPointID,
                       emoji_endpoint_name, kBluetoothMacAddress);
  auto is_valid = ble_advertisement.IsValid();

  EXPECT_TRUE(is_valid);
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndPointID, ble_advertisement.GetEndpointId());
  EXPECT_EQ(emoji_endpoint_name, ble_advertisement.GetEndpointName());
  EXPECT_EQ(kBluetoothMacAddress, ble_advertisement.GetBluetoothMacAddress());
}

TEST(BleAdvertisementTest, ConstructionFailsWithLongEndpointName) {
  std::string long_endpoint_name(BleAdvertisement::kMaxEndpointNameLength + 1,
                                 'x');

  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto ble_advertisement =
      BleAdvertisement(kVersion, kPcp, service_id_hash, kEndPointID,
                       long_endpoint_name, kBluetoothMacAddress);

  auto is_valid = ble_advertisement.IsValid();

  EXPECT_FALSE(is_valid);
}

TEST(BleAdvertisementTest, ConstructionFailsWithBadVersion) {
  auto bad_version = static_cast<BleAdvertisement::Version>(666);

  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto ble_advertisement =
      BleAdvertisement(bad_version, kPcp, service_id_hash, kEndPointID,
                       kEndpointName, kBluetoothMacAddress);

  auto is_valid = ble_advertisement.IsValid();

  EXPECT_FALSE(is_valid);
}

TEST(BleAdvertisementTest, ConstructionFailsWithBadPCP) {
  auto bad_pcp = static_cast<Pcp>(666);

  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto ble_advertisement =
      BleAdvertisement(kVersion, bad_pcp, service_id_hash, kEndPointID,
                       kEndpointName, kBluetoothMacAddress);

  auto is_valid = ble_advertisement.IsValid();

  EXPECT_FALSE(is_valid);
}

TEST(BleAdvertisementTest, ConstructionSucceedsWithEmptyBluetoothMacAddress) {
  std::string empty_bluetooth_mac_address = "";

  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto ble_advertisement =
      BleAdvertisement(kVersion, kPcp, service_id_hash, kEndPointID,
                       kEndpointName, empty_bluetooth_mac_address);

  auto is_valid = ble_advertisement.IsValid();

  EXPECT_TRUE(is_valid);
}

TEST(BleAdvertisementTest, ConstructionSucceedsWithInvalidBluetoothMacAddress) {
  std::string bad_bluetooth_mac_address = "022:00";

  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto ble_advertisement =
      BleAdvertisement(kVersion, kPcp, service_id_hash, kEndPointID,
                       kEndpointName, bad_bluetooth_mac_address);
  auto is_valid = ble_advertisement.IsValid();

  EXPECT_TRUE(is_valid);
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndPointID, ble_advertisement.GetEndpointId());
  EXPECT_EQ(kEndpointName, ble_advertisement.GetEndpointName());
  EXPECT_TRUE(ble_advertisement.GetBluetoothMacAddress().empty());
}

TEST(BleAdvertisementTest, ConstructionFromBytesWorks) {
  // Serialize good data into a good Ble Advertisement.
  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto org_ble_advertisement =
      BleAdvertisement(kVersion, kPcp, service_id_hash, kEndPointID,
                       kEndpointName, kBluetoothMacAddress);
  auto ble_advertisement_bytes = ByteArray(org_ble_advertisement);

  auto ble_advertisement = BleAdvertisement(ble_advertisement_bytes);
  auto is_valid = ble_advertisement.IsValid();

  EXPECT_TRUE(is_valid);
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndPointID, ble_advertisement.GetEndpointId());
  EXPECT_EQ(kEndpointName, ble_advertisement.GetEndpointName());
  EXPECT_EQ(kBluetoothMacAddress, ble_advertisement.GetBluetoothMacAddress());
}

// Bytes at the end should be ignored so that they can be used as reserve bytes
// in the future.
TEST(BleAdvertisementTest, ConstructionFromLongLengthBytesWorks) {
  // Serialize good data into a good Ble Advertisement.
  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto ble_advertisement =
      BleAdvertisement(kVersion, kPcp, service_id_hash, kEndPointID,
                       kEndpointName, kBluetoothMacAddress);
  auto ble_advertisement_bytes = ByteArray(ble_advertisement);

  // Add bytes to the end of the valid Ble advertisement.
  auto long_ble_advertisement_bytes =
      ByteArray(BleAdvertisement::kMinAdvertisementLength + 1000);
  ASSERT_LE(ble_advertisement_bytes.size(),
            long_ble_advertisement_bytes.size());
  memcpy(long_ble_advertisement_bytes.data(),
         ble_advertisement_bytes.data(),
         ble_advertisement_bytes.size());

  auto long_ble_advertisement = BleAdvertisement(long_ble_advertisement_bytes);
  auto is_valid = long_ble_advertisement.IsValid();

  EXPECT_TRUE(is_valid);
  EXPECT_EQ(kVersion, long_ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, long_ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, long_ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndPointID, long_ble_advertisement.GetEndpointId());
  EXPECT_EQ(kEndpointName, long_ble_advertisement.GetEndpointName());
  EXPECT_EQ(kBluetoothMacAddress,
            long_ble_advertisement.GetBluetoothMacAddress());
}

TEST(BleAdvertisementTest, ConstructionFromNullBytesFails) {
  auto ble_advertisement = BleAdvertisement(ByteArray());
  auto is_valid = ble_advertisement.IsValid();

  EXPECT_FALSE(is_valid);
}

TEST(BleAdvertisementTest, ConstructionFromShortLengthBytesFails) {
  // Serialize good data into a good Ble Advertisement.
  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto ble_advertisement =
      BleAdvertisement(kVersion, kPcp, service_id_hash, kEndPointID,
                       kEndpointName, kBluetoothMacAddress);
  auto ble_advertisement_bytes = ByteArray(ble_advertisement);

  // Shorten the valid Ble Advertisement.
  auto short_ble_advertisement_bytes(
      ByteArray(ble_advertisement_bytes.data(),
                BleAdvertisement::kMinAdvertisementLength - 1));

  auto short_ble_advertisement =
      BleAdvertisement(short_ble_advertisement_bytes);
  auto is_valid = short_ble_advertisement.IsValid();

  EXPECT_FALSE(is_valid);
}

TEST(BleAdvertisementTest,
     ConstructionFromByesWithWrongEndpointNameLengthFails) {
  // Serialize good data into a good Ble Advertisement.
  auto service_id_hash = ByteArray(kServiceIDHashBytes,
                                   sizeof(kServiceIDHashBytes) / sizeof(char));
  auto ble_advertisement =
      BleAdvertisement(kVersion, kPcp, service_id_hash, kEndPointID,
                       kEndpointName, kBluetoothMacAddress);
  auto ble_advertisement_bytes = ByteArray(ble_advertisement);

  // Corrupt the EndpointNameLength bits.
  std::string corrupt_ble_advertisement_string(ble_advertisement_bytes.data(),
                                               ble_advertisement_bytes.size());
  corrupt_ble_advertisement_string[8] ^= 0x0FF;
  auto corrupt_ble_advertisement_bytes =
      ByteArray(corrupt_ble_advertisement_string);

  auto corrupt_ble_advertisement =
      BleAdvertisement(corrupt_ble_advertisement_bytes);
  auto is_valid = corrupt_ble_advertisement.IsValid();

  EXPECT_FALSE(is_valid);
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
