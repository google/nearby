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

#include "core/internal/ble_advertisement.h"

#include <cstring>

#include "platform/port/string.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

const BLEAdvertisement::Version::Value version = BLEAdvertisement::Version::V1;
const PCP::Value pcp = PCP::P2P_CLUSTER;
const char endpoint_id[] = "AB12";
const char service_id_hash_bytes[] = {0x0A, 0x0B, 0x0C};
const char endpoint_name[] =
    "How much wood can a woodchuck chuck if a wood chuck would chuck wood?";
const char bluetooth_mac_address[] = "00:00:E6:88:64:13";

TEST(BLEAdvertisementTest, SerializationDeserializationWorks) {
  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, pcp, ConstifyPtr(scoped_service_id_hash.get()), endpoint_id,
          endpoint_name, bluetooth_mac_address));
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));

  ASSERT_EQ(pcp, scoped_ble_advertisement->getPCP());
  ASSERT_EQ(version, scoped_ble_advertisement->getVersion());
  ASSERT_EQ(endpoint_id, scoped_ble_advertisement->getEndpointId());
  ASSERT_EQ(sizeof(service_id_hash_bytes) / sizeof(char),
            scoped_ble_advertisement->getServiceIdHash()->size());
  ASSERT_EQ(0, memcmp(service_id_hash_bytes,
                      scoped_ble_advertisement->getServiceIdHash()->getData(),
                      scoped_ble_advertisement->getServiceIdHash()->size()));
  ASSERT_EQ(endpoint_name, scoped_ble_advertisement->getEndpointName());
  ASSERT_EQ(bluetooth_mac_address,
            scoped_ble_advertisement->getBluetoothMacAddress());
}

TEST(BLEAdvertisementTest, SerializationDeserializationWorksWithGoodPCP) {
  PCP::Value good_pcp = PCP::P2P_STAR;

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, good_pcp, ConstifyPtr(scoped_service_id_hash.get()),
          endpoint_id, endpoint_name, bluetooth_mac_address));
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));

  ASSERT_EQ(good_pcp, scoped_ble_advertisement->getPCP());
  ASSERT_EQ(version, scoped_ble_advertisement->getVersion());
  ASSERT_EQ(endpoint_id, scoped_ble_advertisement->getEndpointId());
  ASSERT_EQ(sizeof(service_id_hash_bytes) / sizeof(char),
            scoped_ble_advertisement->getServiceIdHash()->size());
  ASSERT_EQ(0, memcmp(service_id_hash_bytes,
                      scoped_ble_advertisement->getServiceIdHash()->getData(),
                      scoped_ble_advertisement->getServiceIdHash()->size()));
  ASSERT_EQ(endpoint_name, scoped_ble_advertisement->getEndpointName());
  ASSERT_EQ(bluetooth_mac_address,
            scoped_ble_advertisement->getBluetoothMacAddress());
}

TEST(BLEAdvertisementTest,
     SerializationDeserializationWorksWithEmptyEndpointName) {
  std::string empty_endpoint_name;

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, pcp, ConstifyPtr(scoped_service_id_hash.get()), endpoint_id,
          empty_endpoint_name, bluetooth_mac_address));
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));

  ASSERT_EQ(pcp, scoped_ble_advertisement->getPCP());
  ASSERT_EQ(version, scoped_ble_advertisement->getVersion());
  ASSERT_EQ(endpoint_id, scoped_ble_advertisement->getEndpointId());
  ASSERT_EQ(sizeof(service_id_hash_bytes) / sizeof(char),
            scoped_ble_advertisement->getServiceIdHash()->size());
  ASSERT_EQ(0, memcmp(service_id_hash_bytes,
                      scoped_ble_advertisement->getServiceIdHash()->getData(),
                      scoped_ble_advertisement->getServiceIdHash()->size()));
  ASSERT_EQ(empty_endpoint_name, scoped_ble_advertisement->getEndpointName());
  ASSERT_EQ(bluetooth_mac_address,
            scoped_ble_advertisement->getBluetoothMacAddress());
}

TEST(BLEAdvertisementTest,
     SerializationDeSerializationFailsWithLongEndpointName) {
  std::string long_endpoint_name(BLEAdvertisement::kMaxEndpointNameLength + 1,
                                 'x');

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, pcp, ConstifyPtr(scoped_service_id_hash.get()), endpoint_id,
          long_endpoint_name, bluetooth_mac_address));

  ASSERT_TRUE(scoped_ble_advertisement_bytes.get().isNull());
}

TEST(BLEAdvertisementTest,
     SerializationDeserializationWorksWithEmojiEndpointName) {
  std::string emoji_endpoint_name("\u0001F450 \u0001F450");

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, pcp, ConstifyPtr(scoped_service_id_hash.get()), endpoint_id,
          emoji_endpoint_name, bluetooth_mac_address));
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));

  ASSERT_EQ(pcp, scoped_ble_advertisement->getPCP());
  ASSERT_EQ(version, scoped_ble_advertisement->getVersion());
  ASSERT_EQ(endpoint_id, scoped_ble_advertisement->getEndpointId());
  ASSERT_EQ(sizeof(service_id_hash_bytes) / sizeof(char),
            scoped_ble_advertisement->getServiceIdHash()->size());
  ASSERT_EQ(0, memcmp(service_id_hash_bytes,
                      scoped_ble_advertisement->getServiceIdHash()->getData(),
                      scoped_ble_advertisement->getServiceIdHash()->size()));
  ASSERT_EQ(emoji_endpoint_name, scoped_ble_advertisement->getEndpointName());
  ASSERT_EQ(bluetooth_mac_address,
            scoped_ble_advertisement->getBluetoothMacAddress());
}

TEST(BLEAdvertisementTest, SerializationFailsWithBadVersion) {
  BLEAdvertisement::Version::Value bad_version =
      static_cast<BLEAdvertisement::Version::Value>(666);

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          bad_version, pcp, ConstifyPtr(scoped_service_id_hash.get()),
          endpoint_id, endpoint_name, bluetooth_mac_address));

  ASSERT_TRUE(scoped_ble_advertisement_bytes.get().isNull());
}

TEST(BLEAdvertisementTest, SerializationFailsWithBadPCP) {
  PCP::Value bad_pcp = static_cast<PCP::Value>(666);

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, bad_pcp, ConstifyPtr(scoped_service_id_hash.get()),
          endpoint_id, endpoint_name, bluetooth_mac_address));

  ASSERT_TRUE(scoped_ble_advertisement_bytes.get().isNull());
}

TEST(BLEAdvertisementTest, SerializationSucceedsWithEmptyBluetoothMacAddress) {
  std::string empty_bluetooth_mac_address = "";

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, pcp, ConstifyPtr(scoped_service_id_hash.get()), endpoint_id,
          endpoint_name, empty_bluetooth_mac_address));
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));

  ASSERT_EQ(pcp, scoped_ble_advertisement->getPCP());
  ASSERT_EQ(version, scoped_ble_advertisement->getVersion());
  ASSERT_EQ(endpoint_id, scoped_ble_advertisement->getEndpointId());
  ASSERT_EQ(sizeof(service_id_hash_bytes) / sizeof(char),
            scoped_ble_advertisement->getServiceIdHash()->size());
  ASSERT_EQ(0, memcmp(service_id_hash_bytes,
                      scoped_ble_advertisement->getServiceIdHash()->getData(),
                      scoped_ble_advertisement->getServiceIdHash()->size()));
  ASSERT_EQ(endpoint_name, scoped_ble_advertisement->getEndpointName());
  ASSERT_EQ(empty_bluetooth_mac_address,
            scoped_ble_advertisement->getBluetoothMacAddress());
}

TEST(BLEAdvertisementTest,
     SerializationSucceedsWithInvalidBluetoothMacAddress) {
  std::string bad_bluetooth_mac_address = "022:00";

  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, pcp, ConstifyPtr(scoped_service_id_hash.get()), endpoint_id,
          endpoint_name, bad_bluetooth_mac_address));
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));

  ASSERT_EQ(pcp, scoped_ble_advertisement->getPCP());
  ASSERT_EQ(version, scoped_ble_advertisement->getVersion());
  ASSERT_EQ(endpoint_id, scoped_ble_advertisement->getEndpointId());
  ASSERT_EQ(sizeof(service_id_hash_bytes) / sizeof(char),
            scoped_ble_advertisement->getServiceIdHash()->size());
  ASSERT_EQ(0, memcmp(service_id_hash_bytes,
                      scoped_ble_advertisement->getServiceIdHash()->getData(),
                      scoped_ble_advertisement->getServiceIdHash()->size()));
  ASSERT_EQ(endpoint_name, scoped_ble_advertisement->getEndpointName());
  ASSERT_TRUE(scoped_ble_advertisement->getBluetoothMacAddress().empty());
}

TEST(BLEAdvertisementTest, DeserializationFailsWithNullBytes) {
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(ConstPtr<ByteArray>()));

  ASSERT_TRUE(scoped_ble_advertisement.get().isNull());
}

TEST(BLEAdvertisementTest, DeserializationFailsWithShortLength) {
  // Serialize good data into a good BLE Advertisement.
  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, pcp, ConstifyPtr(scoped_service_id_hash.get()), endpoint_id,
          endpoint_name, bluetooth_mac_address));

  // Shorten the valid BLE Advertisement.
  ScopedPtr<ConstPtr<ByteArray> > short_ble_advertisement_bytes(MakeConstPtr(
      new ByteArray(scoped_ble_advertisement_bytes.get()->getData(),
                    BLEAdvertisement::kMinAdvertisementLength - 1)));

  // Fail to deserialize the short BLE Advertisement.
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_short_ble_advertisement(
      BLEAdvertisement::fromBytes(short_ble_advertisement_bytes.get()));
  ASSERT_TRUE(scoped_short_ble_advertisement.get().isNull());

  // Make sure deserialization succeeds with the valid BLE Advertisement.
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));
  ASSERT_FALSE(scoped_ble_advertisement.get().isNull());
}

TEST(BLEAdvertisementTest, DeserializationFailsWithWrongEndpointNameLength) {
  // Serialize good data into a good BLE Advertisement.
  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, pcp, ConstifyPtr(scoped_service_id_hash.get()), endpoint_id,
          endpoint_name, bluetooth_mac_address));

  // Corrupt the EndpointNameLength bits.
  std::string corrupt_ble_advertisement_bytes(
      scoped_ble_advertisement_bytes->getData(),
      scoped_ble_advertisement_bytes->size());
  corrupt_ble_advertisement_bytes[8] ^= 0x0FF;
  ScopedPtr<ConstPtr<ByteArray> > scoped_corrupt_ble_advertisement_bytes(
      MakeConstPtr(new ByteArray(corrupt_ble_advertisement_bytes.data(),
                                 corrupt_ble_advertisement_bytes.size())));

  // And deserialize the corrupt BLE Advertisement.
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(
          scoped_corrupt_ble_advertisement_bytes.get()));
  ASSERT_TRUE(scoped_ble_advertisement.isNull());
}

// Bytes at the end should be ignored so that they can be used as reserve bytes
// in the future.
TEST(BLEAdvertisementTest, DeserializationPassesWithLongLength) {
  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, pcp, ConstifyPtr(scoped_service_id_hash.get()), endpoint_id,
          endpoint_name, bluetooth_mac_address));

  // Add bytes to the end of the valid BLE advertisement.
  auto new_array =
      new ByteArray(BLEAdvertisement::kMinAdvertisementLength + 1000);
  ASSERT_LE(scoped_ble_advertisement_bytes->size(), new_array->size());
  memcpy(new_array->getData(),
         scoped_ble_advertisement_bytes->getData(),
         scoped_ble_advertisement_bytes->size());
  ScopedPtr<ConstPtr<ByteArray> > long_ble_advertisement_bytes(MakeConstPtr(
      new_array));

  // Deserialize the long BLE advertisement.
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_long_ble_advertisement(
      BLEAdvertisement::fromBytes(long_ble_advertisement_bytes.get()));
  ASSERT_FALSE(scoped_long_ble_advertisement.get().isNull());

  // Make sure deserialization succeeds with the valid BLE Advertisement.
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));
  ASSERT_FALSE(scoped_ble_advertisement.get().isNull());
}

TEST(BLEAdvertisementTest, DeserializationWorksWithLongEndpointName) {
  // Serialize good data into a good BLE Advertisement.
  ScopedPtr<Ptr<ByteArray> > scoped_service_id_hash(new ByteArray(
      service_id_hash_bytes, sizeof(service_id_hash_bytes) / sizeof(char)));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          version, pcp, ConstifyPtr(scoped_service_id_hash.get()), endpoint_id,
          endpoint_name, bluetooth_mac_address));

  // Corrupt the EndpointNameLength bits and increase it past the accepted max
  // length.
  std::string corrupt_ble_advertisement_bytes(
      scoped_ble_advertisement_bytes->getData(),
      scoped_ble_advertisement_bytes->size());
  corrupt_ble_advertisement_bytes[8] ^=
      BLEAdvertisement::kMaxEndpointNameLength + 10;
  ScopedPtr<ConstPtr<ByteArray> > scoped_corrupt_ble_advertisement_bytes(
      MakeConstPtr(new ByteArray(corrupt_ble_advertisement_bytes.data(),
                                 corrupt_ble_advertisement_bytes.size())));
  // Increase the size of the advertisement so that there's enough data for the
  // now-longer endpoint name.
  auto new_array =
      new ByteArray(BLEAdvertisement::kMinAdvertisementLength + 1000);
  ASSERT_LE(scoped_ble_advertisement_bytes->size(), new_array->size());
  memcpy(new_array->getData(),
         scoped_ble_advertisement_bytes->getData(),
         scoped_ble_advertisement_bytes->size());
  ScopedPtr<ConstPtr<ByteArray> > long_ble_advertisement_bytes(MakeConstPtr(
      new_array));

  // And deserialize the changed BLE Advertisement.
  ScopedPtr<Ptr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(long_ble_advertisement_bytes.get()));
  ASSERT_FALSE(scoped_ble_advertisement.isNull());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
