#include "core_v2/internal/mediums/ble_packet.h"

#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

constexpr char kServiceIDHash[] = "\x0a\x0b\x0c";
constexpr char kData[] = "\x01\x02\x03\x04\x05";

TEST(BlePacketTest, ConstructionWorks) {
  ByteArray service_id_hash(kServiceIDHash);
  ByteArray data(kData);

  BlePacket ble_packet(service_id_hash, data);

  EXPECT_TRUE(ble_packet.IsValid());
  EXPECT_EQ(service_id_hash, ble_packet.GetServiceIdHash());
  EXPECT_EQ(data, ble_packet.GetData());
}

TEST(BlePacketTest, ConstructionWorksWithEmptyData) {
  char empty_data[] = {};

  ByteArray service_id_hash(kServiceIDHash);
  ByteArray data(empty_data);

  BlePacket ble_packet(service_id_hash, data);

  EXPECT_TRUE(ble_packet.IsValid());
  EXPECT_EQ(service_id_hash, ble_packet.GetServiceIdHash());
  EXPECT_EQ(data, ble_packet.GetData());
}

TEST(BlePacketTest, ConstructionFailsWithShortServiceIdHash) {
  char short_service_id_hash[] = "\x0a\x0b";

  ByteArray service_id_hash(short_service_id_hash);
  ByteArray data(kData);

  BlePacket ble_packet(service_id_hash, data);

  EXPECT_FALSE(ble_packet.IsValid());
}

TEST(BlePacketTest, ConstructionFailsWithLongServiceIdHash) {
  char long_service_id_hash[] = "\x0a\x0b\x0c\x0d";

  ByteArray service_id_hash(long_service_id_hash);
  ByteArray data(kData);

  BlePacket ble_packet(service_id_hash, data);

  EXPECT_FALSE(ble_packet.IsValid());
}

TEST(BlePacketTest, ConstructionFromSerializedBytesWorks) {
  ByteArray service_id_hash(kServiceIDHash);
  ByteArray data(kData);

  BlePacket org_ble_packet(service_id_hash, data);
  ByteArray ble_packet_bytes(org_ble_packet);

  BlePacket ble_packet(ble_packet_bytes);

  EXPECT_TRUE(ble_packet.IsValid());
  EXPECT_EQ(service_id_hash, ble_packet.GetServiceIdHash());
  EXPECT_EQ(data, ble_packet.GetData());
}

TEST(BlePacketTest, ConstructionFromNullBytesFails) {
  BlePacket ble_packet(ByteArray{});

  EXPECT_FALSE(ble_packet.IsValid());
}

TEST(BlePacketTest, ConstructionFromShortLengthDataFails) {
  ByteArray service_id_hash(kServiceIDHash);
  ByteArray data(kData);

  BlePacket org_ble_packet(service_id_hash, data);
  ByteArray org_ble_packet_bytes(org_ble_packet);

  // Cut off the packet so that it's too short
  ByteArray short_ble_packet_bytes(ByteArray(org_ble_packet_bytes.data(), 2));

  BlePacket short_ble_packet(short_ble_packet_bytes);

  EXPECT_FALSE(short_ble_packet.IsValid());
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
