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

#include "core/internal/mediums/ble_advertisement.h"

#include <algorithm>

#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {
namespace {

const BLEAdvertisement::Version::Value kVersion = BLEAdvertisement::Version::V2;
const BLEAdvertisement::SocketVersion::Value kSocketVersion =
    BLEAdvertisement::SocketVersion::V2;
const char kServiceIDHashBytes[] = {0x0A, 0x0B, 0x0C};
const char kData[] =
    "How much wood can a woodchuck chuck if a wood chuck would chuck wood?";
// This corresponds to the length of a specific BLEAdvertisement packed with the
// kData given above. Be sure to update this if kData ever changes.
const size_t kAdvertisementLength = 77;
const size_t kLongAdvertisementLength = kAdvertisementLength + 1000;

TEST(BLEAdvertisementTest, SerializationDeserializationWorksV1) {
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
      MakeConstPtr(new ByteArray(kServiceIDHashBytes,
                                 sizeof(kServiceIDHashBytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(
          BLEAdvertisement::Version::V1, BLEAdvertisement::SocketVersion::V1,
          scoped_service_id_hash.get(), scoped_data.get()));
  ScopedPtr<ConstPtr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));

  ASSERT_EQ(BLEAdvertisement::Version::V1,
            scoped_ble_advertisement->getVersion());
  ASSERT_EQ(BLEAdvertisement::SocketVersion::V1,
            scoped_ble_advertisement->getSocketVersion());
  ASSERT_EQ(scoped_service_id_hash->size(),
            scoped_ble_advertisement->getServiceIdHash()->size());
  ASSERT_EQ(0, memcmp(kServiceIDHashBytes,
                      scoped_ble_advertisement->getServiceIdHash()->getData(),
                      scoped_ble_advertisement->getServiceIdHash()->size()));
  ASSERT_EQ(scoped_data->size(), scoped_ble_advertisement->getData()->size());
  ASSERT_EQ(0, memcmp(kData, scoped_ble_advertisement->getData()->getData(),
                      scoped_ble_advertisement->getData()->size()));
}

TEST(BLEAdvertisementTest, SerializationDeserializationWorks) {
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
      MakeConstPtr(new ByteArray(kServiceIDHashBytes,
                                 sizeof(kServiceIDHashBytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(kVersion, kSocketVersion,
                                scoped_service_id_hash.get(),
                                scoped_data.get()));
  ScopedPtr<ConstPtr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));

  ASSERT_EQ(kVersion, scoped_ble_advertisement->getVersion());
  ASSERT_EQ(kSocketVersion, scoped_ble_advertisement->getSocketVersion());
  ASSERT_EQ(scoped_service_id_hash->size(),
            scoped_ble_advertisement->getServiceIdHash()->size());
  ASSERT_EQ(0, memcmp(kServiceIDHashBytes,
                      scoped_ble_advertisement->getServiceIdHash()->getData(),
                      scoped_ble_advertisement->getServiceIdHash()->size()));
  ASSERT_EQ(scoped_data->size(), scoped_ble_advertisement->getData()->size());
  ASSERT_EQ(0, memcmp(kData, scoped_ble_advertisement->getData()->getData(),
                      scoped_ble_advertisement->getData()->size()));
}

TEST(BLEAdvertisementTest, SerializationDeserializationWorksWithEmptyData) {
  char empty_data[0];

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
      MakeConstPtr(new ByteArray(kServiceIDHashBytes,
                                 sizeof(kServiceIDHashBytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(MakeConstPtr(
      new ByteArray(empty_data, sizeof(empty_data) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(kVersion, kSocketVersion,
                                scoped_service_id_hash.get(),
                                scoped_data.get()));
  ScopedPtr<ConstPtr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));

  ASSERT_EQ(kVersion, scoped_ble_advertisement->getVersion());
  ASSERT_EQ(kSocketVersion, scoped_ble_advertisement->getSocketVersion());
  ASSERT_EQ(scoped_service_id_hash->size(),
            scoped_ble_advertisement->getServiceIdHash()->size());
  ASSERT_EQ(0, memcmp(kServiceIDHashBytes,
                      scoped_ble_advertisement->getServiceIdHash()->getData(),
                      scoped_ble_advertisement->getServiceIdHash()->size()));
  ASSERT_EQ(scoped_data->size(), scoped_ble_advertisement->getData()->size());
  ASSERT_EQ(0, memcmp(kData, scoped_ble_advertisement->getData()->getData(),
                      scoped_ble_advertisement->getData()->size()));
}

TEST(BLEAdvertisementTest, SerializationDeserializationFailsWithLargeData) {
  // Create data that's larger than the allowed size.
  char large_data[513];

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
      MakeConstPtr(new ByteArray(kServiceIDHashBytes,
                                 sizeof(kServiceIDHashBytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(MakeConstPtr(
      new ByteArray(large_data, sizeof(large_data) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(kVersion, kSocketVersion,
                                scoped_service_id_hash.get(),
                                scoped_data.get()));
  ScopedPtr<ConstPtr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_ble_advertisement_bytes.get()));

  ASSERT_TRUE(scoped_ble_advertisement.isNull());
}

TEST(BLEAdvertisementTest, SerializationFailsWithBadVersion) {
  BLEAdvertisement::Version::Value bad_version =
      static_cast<BLEAdvertisement::Version::Value>(666);

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
      MakeConstPtr(new ByteArray(kServiceIDHashBytes,
                                 sizeof(kServiceIDHashBytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(bad_version, kSocketVersion,
                                scoped_service_id_hash.get(),
                                scoped_data.get()));

  ASSERT_TRUE(scoped_ble_advertisement_bytes.isNull());
}

TEST(BLEAdvertisementTest, SerializationFailsWithBadSocketVersion) {
  BLEAdvertisement::SocketVersion::Value bad_socket_version =
      static_cast<BLEAdvertisement::SocketVersion::Value>(666);

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
      MakeConstPtr(new ByteArray(kServiceIDHashBytes,
                                 sizeof(kServiceIDHashBytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(kVersion, bad_socket_version,
                                scoped_service_id_hash.get(),
                                scoped_data.get()));

  ASSERT_TRUE(scoped_ble_advertisement_bytes.isNull());
}

TEST(BLEAdvertisementTest, SerializationFailsWithShortServiceIdHash) {
  char short_service_id_hash_bytes[] = {0x0A, 0x0B};

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(MakeConstPtr(
      new ByteArray(short_service_id_hash_bytes,
                    sizeof(short_service_id_hash_bytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(kVersion, kSocketVersion,
                                scoped_service_id_hash.get(),
                                scoped_data.get()));

  ASSERT_TRUE(scoped_ble_advertisement_bytes.isNull());
}

TEST(BLEAdvertisementTest, SerializationFailsWithLongServiceIdHash) {
  char long_service_id_hash_bytes[] = {0x0A, 0x0B, 0x0C, 0x0D};

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(MakeConstPtr(
      new ByteArray(long_service_id_hash_bytes,
                    sizeof(long_service_id_hash_bytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(kVersion, kSocketVersion,
                                scoped_service_id_hash.get(),
                                scoped_data.get()));

  ASSERT_TRUE(scoped_ble_advertisement_bytes.isNull());
}

TEST(BLEAdvertisementTest, SerializationFailsWithLongData) {
  // BLEAdvertisement shouldn't be able to support data with the max GATT
  // attribute length because it needs some room for the preceding fields.
  char long_data[512];

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
      MakeConstPtr(new ByteArray(kServiceIDHashBytes,
                                 sizeof(kServiceIDHashBytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(long_data, sizeof(long_data) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(kVersion, kSocketVersion,
                                scoped_service_id_hash.get(),
                                scoped_data.get()));

  ASSERT_TRUE(scoped_ble_advertisement_bytes.isNull());
}

TEST(BLEAdvertisementTest, DeserializationWorksWithExtraBytes) {
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
      MakeConstPtr(new ByteArray(kServiceIDHashBytes,
                                 sizeof(kServiceIDHashBytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(kVersion, kSocketVersion,
                                scoped_service_id_hash.get(),
                                scoped_data.get()));

  // Copy the bytes into a new array with extra bytes. We must explicitly
  // define how long our array is because we can't use variable length arrays.
  char raw_ble_advertisement_bytes[kLongAdvertisementLength] {};
  memcpy(raw_ble_advertisement_bytes, scoped_ble_advertisement_bytes->getData(),
         std::min(sizeof(raw_ble_advertisement_bytes),
                  scoped_ble_advertisement_bytes->size()));

  // Re-parse the BLE advertisement using our extra long advertisement bytes.
  ScopedPtr<ConstPtr<ByteArray> > scoped_long_ble_advertisement_bytes(
      MakeConstPtr(new ByteArray(raw_ble_advertisement_bytes,
                                 kLongAdvertisementLength)));
  ScopedPtr<ConstPtr<BLEAdvertisement> > scoped_long_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_long_ble_advertisement_bytes.get()));

  ASSERT_EQ(kVersion, scoped_long_ble_advertisement->getVersion());
  ASSERT_EQ(kSocketVersion, scoped_long_ble_advertisement->getSocketVersion());
  ASSERT_EQ(scoped_service_id_hash->size(),
            scoped_long_ble_advertisement->getServiceIdHash()->size());
  ASSERT_EQ(0,
            memcmp(kServiceIDHashBytes,
                   scoped_long_ble_advertisement->getServiceIdHash()->getData(),
                   scoped_long_ble_advertisement->getServiceIdHash()->size()));
  ASSERT_EQ(scoped_data->size(),
            scoped_long_ble_advertisement->getData()->size());
  ASSERT_EQ(0,
            memcmp(kData, scoped_long_ble_advertisement->getData()->getData(),
                   scoped_long_ble_advertisement->getData()->size()));
}

TEST(BLEAdvertisementTest, DeserializationFailsWithNullBytes) {
  ScopedPtr<ConstPtr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(ConstPtr<ByteArray>()));

  ASSERT_TRUE(scoped_ble_advertisement.isNull());
}

TEST(BLEAdvertisementTest, DeserializationFailsWithShortLength) {
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
      MakeConstPtr(new ByteArray(kServiceIDHashBytes,
                                 sizeof(kServiceIDHashBytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(kVersion, kSocketVersion,
                                scoped_service_id_hash.get(),
                                scoped_data.get()));

  // Cut off the advertisement so that it's too short.
  ScopedPtr<ConstPtr<ByteArray> > scoped_short_ble_advertisement_bytes(
      MakeConstPtr(
          new ByteArray(scoped_ble_advertisement_bytes->getData(), 7)));
  ScopedPtr<ConstPtr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(scoped_short_ble_advertisement_bytes.get()));

  ASSERT_TRUE(scoped_ble_advertisement.isNull());
}

TEST(BLEAdvertisementTest, DeserializationFailsWithInvalidDataLength) {
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_hash(
      MakeConstPtr(new ByteArray(kServiceIDHashBytes,
                                 sizeof(kServiceIDHashBytes) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_data(
      MakeConstPtr(new ByteArray(kData, sizeof(kData) / sizeof(char))));

  ScopedPtr<ConstPtr<ByteArray> > scoped_ble_advertisement_bytes(
      BLEAdvertisement::toBytes(kVersion, kSocketVersion,
                                scoped_service_id_hash.get(),
                                scoped_data.get()));

  // Corrupt the DATA_SIZE bits. Start by making a raw copy of the BLE
  // advertisement bytes so we can modify it. We must explicitly define how long
  // our array is because we can't use variable length arrays.
  char raw_ble_advertisement_bytes[kAdvertisementLength];
  memcpy(raw_ble_advertisement_bytes, scoped_ble_advertisement_bytes->getData(),
         kAdvertisementLength);

  // The data size field lives in indices 4-7. Corrupt it.
  memset(raw_ble_advertisement_bytes + 4, 0xFF, 4);

  // Try to parse the BLE advertisement using our corrupted advertisement bytes.
  ScopedPtr<ConstPtr<ByteArray> > scoped_corrupted_ble_advertisement_bytes(
      MakeConstPtr(
          new ByteArray(raw_ble_advertisement_bytes, kAdvertisementLength)));
  ScopedPtr<ConstPtr<BLEAdvertisement> > scoped_ble_advertisement(
      BLEAdvertisement::fromBytes(
          scoped_corrupted_ble_advertisement_bytes.get()));

  ASSERT_TRUE(scoped_ble_advertisement.isNull());
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
