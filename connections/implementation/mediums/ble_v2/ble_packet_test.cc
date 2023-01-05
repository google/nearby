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

#include "connections/implementation/mediums/ble_v2/ble_packet.h"

#include "gtest/gtest.h"

namespace nearby {
namespace connections {
namespace mediums {

constexpr absl::string_view kServiceIDHash{"\x0a\x0b\x0c"};
constexpr absl::string_view kData{"\x01\x02\x03\x04\x05"};

TEST(BlePacketTest, ConstructionWorks) {
  ByteArray service_id_hash{std::string(kServiceIDHash)};
  ByteArray data{std::string(kData)};

  BlePacket ble_packet{service_id_hash, data};

  EXPECT_TRUE(ble_packet.IsValid());
  EXPECT_EQ(service_id_hash, ble_packet.GetServiceIdHash());
  EXPECT_EQ(data, ble_packet.GetData());
}

TEST(BlePacketTest, ConstructionWorksWithEmptyData) {
  char empty_data[] = "";

  ByteArray service_id_hash{std::string(kServiceIDHash)};
  ByteArray data{empty_data};

  BlePacket ble_packet{service_id_hash, data};

  EXPECT_TRUE(ble_packet.IsValid());
  EXPECT_EQ(service_id_hash, ble_packet.GetServiceIdHash());
  EXPECT_EQ(data, ble_packet.GetData());
}

TEST(BlePacketTest, ConstructionFailsWithShortServiceIdHash) {
  char short_service_id_hash[] = "\x0a\x0b";

  ByteArray service_id_hash{short_service_id_hash};
  ByteArray data{std::string(kData)};

  BlePacket ble_packet(service_id_hash, data);

  EXPECT_FALSE(ble_packet.IsValid());
}

TEST(BlePacketTest, ConstructionFailsWithLongServiceIdHash) {
  char long_service_id_hash[] = "\x0a\x0b\x0c\x0d";

  ByteArray service_id_hash{long_service_id_hash};
  ByteArray data{std::string(kData)};

  BlePacket ble_packet{service_id_hash, data};

  EXPECT_FALSE(ble_packet.IsValid());
}

TEST(BlePacketTest, ConstructionFromSerializedBytesWorks) {
  ByteArray service_id_hash{std::string(kServiceIDHash)};
  ByteArray data{std::string(kData)};

  BlePacket org_ble_packet{service_id_hash, data};
  ByteArray ble_packet_bytes{org_ble_packet};

  BlePacket ble_packet{ble_packet_bytes};

  EXPECT_TRUE(ble_packet.IsValid());
  EXPECT_EQ(service_id_hash, ble_packet.GetServiceIdHash());
  EXPECT_EQ(data, ble_packet.GetData());
}

TEST(BlePacketTest, ConstructionFromNullBytesFails) {
  BlePacket ble_packet{ByteArray{}};

  EXPECT_FALSE(ble_packet.IsValid());
}

TEST(BlePacketTest, ConstructionFromShortLengthDataFails) {
  ByteArray service_id_hash{std::string(kServiceIDHash)};
  ByteArray data{std::string(kData)};

  BlePacket org_ble_packet{service_id_hash, data};
  ByteArray org_ble_packet_bytes{org_ble_packet};

  // Cut off the packet so that it's too short
  ByteArray short_ble_packet_bytes{ByteArray{org_ble_packet_bytes.data(), 2}};

  BlePacket short_ble_packet{short_ble_packet_bytes};

  EXPECT_FALSE(short_ble_packet.IsValid());
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
