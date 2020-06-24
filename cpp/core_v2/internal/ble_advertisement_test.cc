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

constexpr BleAdvertisement::Version kVersion = BleAdvertisement::Version::kV1;
constexpr Pcp kPcp = Pcp::kP2pCluster;
constexpr absl::string_view kServiceIDHashBytes{"\x0a\x0b\x0c"};
constexpr absl::string_view kEndPointID{"AB12"};
constexpr absl::string_view kEndpointName{
    "How much wood can a woodchuck chuck if a wood chuck would chuck wood?"};
constexpr absl::string_view kBluetoothMacAddress{"00:00:E6:88:64:13"};

TEST(BleAdvertisementTest, ConstructionWorks) {
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndPointID),
                                     std::string(kEndpointName),
                                     std::string(kBluetoothMacAddress)};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndPointID, ble_advertisement.GetEndpointId());
  EXPECT_EQ(kEndpointName, ble_advertisement.GetEndpointName());
  EXPECT_EQ(kBluetoothMacAddress, ble_advertisement.GetBluetoothMacAddress());
}

TEST(BleAdvertisementTest, ConstructionWorksWithEmptyEndpointName) {
  std::string empty_endpoint_name;

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndPointID),
                                     empty_endpoint_name,
                                     std::string(kBluetoothMacAddress)};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndPointID, ble_advertisement.GetEndpointId());
  EXPECT_EQ(empty_endpoint_name, ble_advertisement.GetEndpointName());
  EXPECT_EQ(kBluetoothMacAddress, ble_advertisement.GetBluetoothMacAddress());
}

TEST(BleAdvertisementTest, ConstructionWorksWithEmojiEndpointName) {
  std::string emoji_endpoint_name{"\u0001F450 \u0001F450"};

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndPointID),
                                     emoji_endpoint_name,
                                     std::string(kBluetoothMacAddress)};

  EXPECT_TRUE(ble_advertisement.IsValid());
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

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndPointID),
                                     long_endpoint_name,
                                     std::string(kBluetoothMacAddress)};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionFailsWithBadVersion) {
  auto bad_version = static_cast<BleAdvertisement::Version>(666);

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement ble_advertisement{bad_version,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndPointID),
                                     std::string(kEndpointName),
                                     std::string(kBluetoothMacAddress)};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionFailsWithBadPCP) {
  auto bad_pcp = static_cast<Pcp>(666);

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     bad_pcp,
                                     service_id_hash,
                                     std::string(kEndPointID),
                                     std::string(kEndpointName),
                                     std::string(kBluetoothMacAddress)};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionSucceedsWithEmptyBluetoothMacAddress) {
  std::string empty_bluetooth_mac_address = "";

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndPointID),
                                     std::string(kEndpointName),
                                     empty_bluetooth_mac_address};

  EXPECT_TRUE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionSucceedsWithInvalidBluetoothMacAddress) {
  std::string bad_bluetooth_mac_address = "022:00";

  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndPointID),
                                     std::string(kEndpointName),
                                     bad_bluetooth_mac_address};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndPointID, ble_advertisement.GetEndpointId());
  EXPECT_EQ(kEndpointName, ble_advertisement.GetEndpointName());
  EXPECT_TRUE(ble_advertisement.GetBluetoothMacAddress().empty());
}

TEST(BleAdvertisementTest, ConstructionFromBytesWorks) {
  // Serialize good data into a good Ble Advertisement.
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement org_ble_advertisement{kVersion,
                                         kPcp,
                                         service_id_hash,
                                         std::string(kEndPointID),
                                         std::string(kEndpointName),
                                         std::string(kBluetoothMacAddress)};
  auto ble_advertisement_bytes = ByteArray(org_ble_advertisement);

  BleAdvertisement ble_advertisement{ble_advertisement_bytes};

  EXPECT_TRUE(ble_advertisement.IsValid());
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
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndPointID),
                                     std::string(kEndpointName),
                                     std::string(kBluetoothMacAddress)};
  auto ble_advertisement_bytes = ByteArray(ble_advertisement);

  // Add bytes to the end of the valid Ble advertisement.
  auto long_ble_advertisement_bytes =
      ByteArray(BleAdvertisement::kMinAdvertisementLength + 1000);
  ASSERT_LE(ble_advertisement_bytes.size(),
            long_ble_advertisement_bytes.size());
  memcpy(long_ble_advertisement_bytes.data(),
         ble_advertisement_bytes.data(),
         ble_advertisement_bytes.size());

  BleAdvertisement long_ble_advertisement{long_ble_advertisement_bytes};

  EXPECT_TRUE(long_ble_advertisement.IsValid());
  EXPECT_EQ(kVersion, long_ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, long_ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, long_ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndPointID, long_ble_advertisement.GetEndpointId());
  EXPECT_EQ(kEndpointName, long_ble_advertisement.GetEndpointName());
  EXPECT_EQ(kBluetoothMacAddress,
            long_ble_advertisement.GetBluetoothMacAddress());
}

TEST(BleAdvertisementTest, ConstructionFromNullBytesFails) {
  BleAdvertisement ble_advertisement{ByteArray{}};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionFromShortLengthBytesFails) {
  // Serialize good data into a good Ble Advertisement.
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndPointID),
                                     std::string(kEndpointName),
                                     std::string(kBluetoothMacAddress)};
  auto ble_advertisement_bytes = ByteArray(ble_advertisement);

  // Shorten the valid Ble Advertisement.
  ByteArray short_ble_advertisement_bytes{
      ble_advertisement_bytes.data(),
      BleAdvertisement::kMinAdvertisementLength - 1};

  BleAdvertisement short_ble_advertisement{short_ble_advertisement_bytes};

  EXPECT_FALSE(short_ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest,
     ConstructionFromByesWithWrongEndpointNameLengthFails) {
  // Serialize good data into a good Ble Advertisement.
  ByteArray service_id_hash{std::string(kServiceIDHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndPointID),
                                     std::string(kEndpointName),
                                     std::string(kBluetoothMacAddress)};
  auto ble_advertisement_bytes = ByteArray(ble_advertisement);

  // Corrupt the EndpointNameLength bits.
  auto corrupt_ble_advertisement_string = std::string(ble_advertisement_bytes);
  corrupt_ble_advertisement_string[8] ^= 0x0FF;
  auto corrupt_ble_advertisement_bytes =
      ByteArray(corrupt_ble_advertisement_string);

  BleAdvertisement corrupt_ble_advertisement{corrupt_ble_advertisement_bytes};

  EXPECT_FALSE(corrupt_ble_advertisement.IsValid());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
