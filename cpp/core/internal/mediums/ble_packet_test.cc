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

#include "core/internal/mediums/ble_packet.h"

#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

const char kServiceIDHash[] = {0x0A, 0x0B, 0x0C};
const char kData[] = {0x00, 0x01, 0x02, 0x03, 0x04};

TEST(BLEPacket, SerializationDeserializationWorks) {
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(MakeConstPtr(
      new ByteArray(kServiceIDHash, sizeof(kServiceIDHash) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_packet_bytes(
      BLEPacket::toBytes(scoped_service_id_hash.get(), scoped_data.get()));
  ScopedPtr<ConstPtr<BLEPacket> > scoped_ble_packet(
      BLEPacket::fromBytes(scoped_ble_packet_bytes.get()));

  ASSERT_EQ(0, memcmp(kServiceIDHash,
                      scoped_ble_packet->getServiceIdHash()->getData(),
                      scoped_ble_packet->getServiceIdHash()->size()));
  ASSERT_EQ(0, memcmp(kData, scoped_ble_packet->getData()->getData(),
                      scoped_ble_packet->getData()->size()));
}

TEST(BLEPacket, SerializationDeserializationWorksWithEmptyData) {
  char empty_data[] = {};

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(MakeConstPtr(
      new ByteArray(kServiceIDHash, sizeof(kServiceIDHash) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(MakeConstPtr(
      new ByteArray(empty_data, sizeof(empty_data) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_packet_bytes(
      BLEPacket::toBytes(scoped_service_id_hash.get(), scoped_data.get()));
  ScopedPtr<ConstPtr<BLEPacket> > scoped_ble_packet(
      BLEPacket::fromBytes(scoped_ble_packet_bytes.get()));

  ASSERT_EQ(0, memcmp(kServiceIDHash,
                      scoped_ble_packet->getServiceIdHash()->getData(),
                      scoped_ble_packet->getServiceIdHash()->size()));
  ASSERT_EQ(0, memcmp(empty_data, scoped_ble_packet->getData()->getData(),
                      scoped_ble_packet->getData()->size()));
}

TEST(BLEPacket, SerializationFailsWithShortServiceIdHash) {
  char short_service_id_hash[] = {0x0A, 0x0B};

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(MakeConstPtr(
      new ByteArray(short_service_id_hash,
                    sizeof(short_service_id_hash) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_packet_bytes(
      BLEPacket::toBytes(scoped_service_id_hash.get(), scoped_data.get()));

  ASSERT_TRUE(scoped_ble_packet_bytes.isNull());
}

TEST(BLEPacket, SerializationFailsWithLongServiceIdHash) {
  char long_service_id_hash[]{0x0A, 0x0B, 0x0C, 0x0D};

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
      MakeConstPtr(new ByteArray(long_service_id_hash,
                                 sizeof(long_service_id_hash) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_packet_bytes(
      BLEPacket::toBytes(scoped_service_id_hash.get(), scoped_data.get()));

  ASSERT_TRUE(scoped_ble_packet_bytes.isNull());
}

TEST(BLEPacket, DeserializationFailsWithNullBytes) {
  ScopedPtr<ConstPtr<BLEPacket> > scoped_ble_packet(
      BLEPacket::fromBytes(ConstPtr<ByteArray>()));

  ASSERT_TRUE(scoped_ble_packet.isNull());
}

TEST(BLEPacket, DeserializationFailsWithShortLength) {
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(MakeConstPtr(
      new ByteArray(kServiceIDHash, sizeof(kServiceIDHash) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_packet_bytes(
      BLEPacket::toBytes(scoped_service_id_hash.get(), scoped_data.get()));

  // Cut off the packet so that it's too short
  ScopedPtr<ConstPtr<ByteArray> > scoped_short_ble_packet_bytes(
      MakeConstPtr(new ByteArray(scoped_ble_packet_bytes->getData(), 2)));
  ScopedPtr<ConstPtr<BLEPacket> > scoped_ble_packet(
      BLEPacket::fromBytes(scoped_short_ble_packet_bytes.get()));

  ASSERT_TRUE(scoped_ble_packet.isNull());
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
