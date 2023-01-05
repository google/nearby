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

#include "connections/implementation/ble_advertisement.h"

#include "gtest/gtest.h"
#include "connections/implementation/base_pcp_handler.h"

namespace nearby {
namespace connections {
namespace {

constexpr BleAdvertisement::Version kVersion = BleAdvertisement::Version::kV1;
constexpr Pcp kPcp = Pcp::kP2pCluster;
constexpr absl::string_view kServiceIdHashBytes{"\x0a\x0b\x0c"};
constexpr absl::string_view kEndpointId{"AB12"};
constexpr absl::string_view kEndpointName{
    "How much wood can a woodchuck chuck if a wood chuck would chuck wood?"};
constexpr absl::string_view kFastAdvertisementEndpointName{"Fast Advertise"};
constexpr absl::string_view kBluetoothMacAddress{"00:00:E6:88:64:13"};
constexpr WebRtcState kWebRtcState = WebRtcState::kConnectable;

// TODO(b/169550050): Implement UWBAddress.
TEST(BleAdvertisementTest, ConstructionWorks) {
  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  ByteArray endpoint_info{std::string(kEndpointName)};
  BleAdvertisement ble_advertisement{
      kVersion,        kPcp,
      service_id_hash, std::string(kEndpointId),
      endpoint_info,   std::string(kBluetoothMacAddress),
      ByteArray{},     kWebRtcState};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_FALSE(ble_advertisement.IsFastAdvertisement());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndpointId, ble_advertisement.GetEndpointId());
  EXPECT_EQ(endpoint_info, ble_advertisement.GetEndpointInfo());
  EXPECT_EQ(kBluetoothMacAddress, ble_advertisement.GetBluetoothMacAddress());
  EXPECT_EQ(kWebRtcState, ble_advertisement.GetWebRtcState());
}

TEST(BleAdvertisementTest, ConstructionWorksForFastAdvertisement) {
  ByteArray fast_endpoint_info{std::string(kFastAdvertisementEndpointName)};
  BleAdvertisement ble_advertisement{kVersion, kPcp, std::string(kEndpointId),
                                     fast_endpoint_info, ByteArray{}};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_TRUE(ble_advertisement.IsFastAdvertisement());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(kEndpointId, ble_advertisement.GetEndpointId());
  EXPECT_EQ(fast_endpoint_info, ble_advertisement.GetEndpointInfo());
  EXPECT_EQ(WebRtcState::kUndefined, ble_advertisement.GetWebRtcState());
}

TEST(BleAdvertisementTest, ConstructionWorksWithEmptyEndpointInfo) {
  ByteArray empty_endpoint_info;

  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndpointId),
                                     empty_endpoint_info,
                                     std::string(kBluetoothMacAddress),
                                     ByteArray{},
                                     kWebRtcState};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_FALSE(ble_advertisement.IsFastAdvertisement());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndpointId, ble_advertisement.GetEndpointId());
  EXPECT_EQ(empty_endpoint_info, ble_advertisement.GetEndpointInfo());
  EXPECT_EQ(kBluetoothMacAddress, ble_advertisement.GetBluetoothMacAddress());
  EXPECT_EQ(kWebRtcState, ble_advertisement.GetWebRtcState());
}

TEST(BleAdvertisementTest,
     ConstructionWorksWithEmptyEndpointInfoForFastAdvertisement) {
  ByteArray empty_endpoint_info;

  BleAdvertisement ble_advertisement{kVersion, kPcp, std::string(kEndpointId),
                                     empty_endpoint_info, ByteArray{}};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_TRUE(ble_advertisement.IsFastAdvertisement());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(kEndpointId, ble_advertisement.GetEndpointId());
  EXPECT_EQ(empty_endpoint_info, ble_advertisement.GetEndpointInfo());
  EXPECT_EQ(WebRtcState::kUndefined, ble_advertisement.GetWebRtcState());
}

TEST(BleAdvertisementTest, ConstructionWorksWithEmojiEndpointInfo) {
  ByteArray emoji_endpoint_info{std::string("\u0001F450 \u0001F450")};

  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  BleAdvertisement ble_advertisement{kVersion,
                                     kPcp,
                                     service_id_hash,
                                     std::string(kEndpointId),
                                     emoji_endpoint_info,
                                     std::string(kBluetoothMacAddress),
                                     ByteArray{},
                                     kWebRtcState};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_FALSE(ble_advertisement.IsFastAdvertisement());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndpointId, ble_advertisement.GetEndpointId());
  EXPECT_EQ(emoji_endpoint_info, ble_advertisement.GetEndpointInfo());
  EXPECT_EQ(kBluetoothMacAddress, ble_advertisement.GetBluetoothMacAddress());
  EXPECT_EQ(kWebRtcState, ble_advertisement.GetWebRtcState());
}

TEST(BleAdvertisementTest,
     ConstructionWorksWithEmojiEndpointInfoForFastAdvertisement) {
  ByteArray emoji_endpoint_info{std::string("\u0001F450 \u0001F450")};

  BleAdvertisement ble_advertisement{kVersion, kPcp, std::string(kEndpointId),
                                     emoji_endpoint_info, ByteArray{}};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_TRUE(ble_advertisement.IsFastAdvertisement());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(kEndpointId, ble_advertisement.GetEndpointId());
  EXPECT_EQ(emoji_endpoint_info, ble_advertisement.GetEndpointInfo());
  EXPECT_EQ(WebRtcState::kUndefined, ble_advertisement.GetWebRtcState());
}

TEST(BleAdvertisementTest, ConstructionFailsWithLongEndpointInfo) {
  std::string long_endpoint_name(BleAdvertisement::kMaxEndpointInfoLength + 1,
                                 'x');
  ByteArray long_endpoint_info{long_endpoint_name};

  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  BleAdvertisement ble_advertisement{
      kVersion,           kPcp,
      service_id_hash,    std::string(kEndpointId),
      long_endpoint_info, std::string(kBluetoothMacAddress),
      ByteArray{},        kWebRtcState};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest,
     ConstructionFailsWithLongEndpointInfoForFastAdvertisement) {
  std::string long_endpoint_name(
      BleAdvertisement::kMaxFastEndpointInfoLength + 1, 'x');
  ByteArray long_endpoint_info{long_endpoint_name};

  BleAdvertisement ble_advertisement{kVersion, kPcp, std::string(kEndpointId),
                                     long_endpoint_info, ByteArray{}};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionFailsWithBadVersion) {
  auto bad_version = static_cast<BleAdvertisement::Version>(666);

  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  ByteArray endpoint_info{std::string(kEndpointName)};
  BleAdvertisement ble_advertisement{
      bad_version,     kPcp,
      service_id_hash, std::string(kEndpointId),
      endpoint_info,   std::string(kBluetoothMacAddress),
      ByteArray{},     kWebRtcState};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest,
     ConstructionFailsWithBadVersionForFastAdvertisement) {
  auto bad_version = static_cast<BleAdvertisement::Version>(666);

  ByteArray fast_endpoint_info{std::string(kFastAdvertisementEndpointName)};
  BleAdvertisement ble_advertisement{bad_version, kPcp,
                                     std::string(kEndpointId),
                                     fast_endpoint_info, ByteArray{}};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionFailsWithBadPCP) {
  auto bad_pcp = static_cast<Pcp>(666);

  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  ByteArray endpoint_info{std::string(kEndpointName)};
  BleAdvertisement ble_advertisement{
      kVersion,        bad_pcp,
      service_id_hash, std::string(kEndpointId),
      endpoint_info,   std::string(kBluetoothMacAddress),
      ByteArray{},     kWebRtcState};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionFailsWithBadPCPForFastAdvertisement) {
  auto bad_pcp = static_cast<Pcp>(666);

  ByteArray fast_endpoint_info{std::string(kFastAdvertisementEndpointName)};
  BleAdvertisement ble_advertisement{kVersion, bad_pcp,
                                     std::string(kEndpointId),
                                     fast_endpoint_info, ByteArray{}};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionSucceedsWithEmptyBluetoothMacAddress) {
  std::string empty_bluetooth_mac_address = "";

  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  ByteArray endpoint_info{std::string(kEndpointName)};
  BleAdvertisement ble_advertisement{
      kVersion,        kPcp,
      service_id_hash, std::string(kEndpointId),
      endpoint_info,   empty_bluetooth_mac_address,
      ByteArray{},     kWebRtcState};

  EXPECT_TRUE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionSucceedsWithInvalidBluetoothMacAddress) {
  std::string bad_bluetooth_mac_address = "022:00";

  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  ByteArray endpoint_info{std::string(kEndpointName)};
  BleAdvertisement ble_advertisement{kVersion,        kPcp,
                                     service_id_hash, std::string(kEndpointId),
                                     endpoint_info,   bad_bluetooth_mac_address,
                                     ByteArray{},     kWebRtcState};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndpointId, ble_advertisement.GetEndpointId());
  EXPECT_EQ(endpoint_info, ble_advertisement.GetEndpointInfo());
  EXPECT_TRUE(ble_advertisement.GetBluetoothMacAddress().empty());
  EXPECT_EQ(kWebRtcState, ble_advertisement.GetWebRtcState());
}

TEST(BleAdvertisementTest, ConstructionFromBytesWorks) {
  // Serialize good data into a good Ble Advertisement.
  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  ByteArray endpoint_info{std::string(kEndpointName)};
  BleAdvertisement org_ble_advertisement{
      kVersion,        kPcp,
      service_id_hash, std::string(kEndpointId),
      endpoint_info,   std::string(kBluetoothMacAddress),
      ByteArray{},     kWebRtcState};
  ByteArray ble_advertisement_bytes(org_ble_advertisement);

  BleAdvertisement ble_advertisement{false, ble_advertisement_bytes};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_FALSE(ble_advertisement.IsFastAdvertisement());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndpointId, ble_advertisement.GetEndpointId());
  EXPECT_EQ(endpoint_info, ble_advertisement.GetEndpointInfo());
  EXPECT_EQ(kBluetoothMacAddress, ble_advertisement.GetBluetoothMacAddress());
  EXPECT_EQ(kWebRtcState, ble_advertisement.GetWebRtcState());
}

TEST(BleAdvertisementTest, ConstructionFromBytesWorksForFastAdvertisement) {
  // Serialize good data into a good Ble Advertisement.
  ByteArray fast_endpoint_info{std::string(kFastAdvertisementEndpointName)};
  BleAdvertisement org_ble_advertisement{kVersion, kPcp,
                                         std::string(kEndpointId),
                                         fast_endpoint_info, ByteArray{}};
  ByteArray ble_advertisement_bytes(org_ble_advertisement);

  BleAdvertisement ble_advertisement{true, ble_advertisement_bytes};

  EXPECT_TRUE(ble_advertisement.IsValid());
  EXPECT_TRUE(ble_advertisement.IsFastAdvertisement());
  EXPECT_EQ(kVersion, ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, ble_advertisement.GetPcp());
  EXPECT_EQ(kEndpointId, ble_advertisement.GetEndpointId());
  EXPECT_EQ(fast_endpoint_info, ble_advertisement.GetEndpointInfo());
  EXPECT_EQ(WebRtcState::kUndefined, ble_advertisement.GetWebRtcState());
}

// Bytes at the end should be ignored so that they can be used as reserve bytes
// in the future.
TEST(BleAdvertisementTest, ConstructionFromLongLengthBytesWorks) {
  // Serialize good data into a good Ble Advertisement.
  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  ByteArray endpoint_info{std::string(kEndpointName)};
  BleAdvertisement ble_advertisement{
      kVersion,        kPcp,
      service_id_hash, std::string(kEndpointId),
      endpoint_info,   std::string(kBluetoothMacAddress),
      ByteArray{},     kWebRtcState};
  ByteArray ble_advertisement_bytes(ble_advertisement);

  // Add bytes to the end of the valid Ble advertisement.
  ByteArray long_ble_advertisement_bytes(
      BleAdvertisement::kMinAdvertisementLength + 1000);
  ASSERT_LE(ble_advertisement_bytes.size(),
            long_ble_advertisement_bytes.size());
  memcpy(long_ble_advertisement_bytes.data(), ble_advertisement_bytes.data(),
         ble_advertisement_bytes.size());

  BleAdvertisement long_ble_advertisement{false, long_ble_advertisement_bytes};

  EXPECT_TRUE(long_ble_advertisement.IsValid());
  EXPECT_EQ(kVersion, long_ble_advertisement.GetVersion());
  EXPECT_EQ(kPcp, long_ble_advertisement.GetPcp());
  EXPECT_EQ(service_id_hash, long_ble_advertisement.GetServiceIdHash());
  EXPECT_EQ(kEndpointId, long_ble_advertisement.GetEndpointId());
  EXPECT_EQ(endpoint_info, long_ble_advertisement.GetEndpointInfo());
  EXPECT_EQ(kBluetoothMacAddress,
            long_ble_advertisement.GetBluetoothMacAddress());
  EXPECT_EQ(kWebRtcState, ble_advertisement.GetWebRtcState());
}

TEST(BleAdvertisementTest, ConstructionFromNullBytesFails) {
  BleAdvertisement ble_advertisement{false, ByteArray{}};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionFromNullBytesFailsForFastAdvertisement) {
  BleAdvertisement ble_advertisement{true, ByteArray{}};

  EXPECT_FALSE(ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest, ConstructionFromShortLengthBytesFails) {
  // Serialize good data into a good Ble Advertisement.
  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  ByteArray endpoint_info{std::string(kEndpointName)};
  BleAdvertisement ble_advertisement{
      kVersion,        kPcp,
      service_id_hash, std::string(kEndpointId),
      endpoint_info,   std::string(kBluetoothMacAddress),
      ByteArray{},     kWebRtcState};
  ByteArray ble_advertisement_bytes(ble_advertisement);

  // Shorten the valid Ble Advertisement.
  ByteArray short_ble_advertisement_bytes{
      ble_advertisement_bytes.data(),
      BleAdvertisement::kMinAdvertisementLength - 1};

  BleAdvertisement short_ble_advertisement{false,
                                           short_ble_advertisement_bytes};

  EXPECT_FALSE(short_ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest,
     ConstructionFromShortLengthBytesFailsForFastAdvertisement) {
  // Serialize good data into a good Ble Advertisement.
  ByteArray fast_endpoint_info{std::string(kFastAdvertisementEndpointName)};
  BleAdvertisement ble_advertisement{kVersion, kPcp, std::string(kEndpointId),
                                     fast_endpoint_info, ByteArray{}};
  ByteArray ble_advertisement_bytes(ble_advertisement);

  // Shorten the valid Ble Advertisement.
  ByteArray short_ble_advertisement_bytes{
      ble_advertisement_bytes.data(),
      BleAdvertisement::kMinAdvertisementLength - 1};

  BleAdvertisement short_ble_advertisement{true, short_ble_advertisement_bytes};

  EXPECT_FALSE(short_ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest,
     ConstructionFromByesWithWrongEndpointInfoLengthFails) {
  // Serialize good data into a good Ble Advertisement.
  ByteArray service_id_hash{std::string(kServiceIdHashBytes)};
  ByteArray endpoint_info{std::string(kEndpointName)};
  BleAdvertisement ble_advertisement{
      kVersion,        kPcp,
      service_id_hash, std::string(kEndpointId),
      endpoint_info,   std::string(kBluetoothMacAddress),
      ByteArray{},     kWebRtcState};
  ByteArray ble_advertisement_bytes(ble_advertisement);

  // Corrupt the EndpointNameLength bits.
  std::string corrupt_ble_advertisement_string(ble_advertisement_bytes);
  corrupt_ble_advertisement_string[8] ^= 0x0FF;
  ByteArray corrupt_ble_advertisement_bytes(corrupt_ble_advertisement_string);

  BleAdvertisement corrupt_ble_advertisement{false,
                                             corrupt_ble_advertisement_bytes};

  EXPECT_FALSE(corrupt_ble_advertisement.IsValid());
}

TEST(BleAdvertisementTest,
     ConstructionFromByesWithWrongEndpointInfoLengthFailsForFastAdvertisement) {
  // Serialize good data into a good Ble Advertisement.
  ByteArray fast_endpoint_info{std::string(kFastAdvertisementEndpointName)};
  BleAdvertisement ble_advertisement{kVersion, kPcp, std::string(kEndpointId),
                                     fast_endpoint_info, ByteArray{}};
  ByteArray ble_advertisement_bytes = ByteArray(ble_advertisement);

  // Corrupt the EndpointInfoLength bits.
  std::string corrupt_ble_advertisement_string(ble_advertisement_bytes);
  corrupt_ble_advertisement_string[5] ^= 0x0FF;
  ByteArray corrupt_ble_advertisement_bytes(corrupt_ble_advertisement_string);

  BleAdvertisement corrupt_ble_advertisement{true,
                                             corrupt_ble_advertisement_bytes};

  EXPECT_FALSE(corrupt_ble_advertisement.IsValid());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
