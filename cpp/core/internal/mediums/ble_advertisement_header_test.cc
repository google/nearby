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

#include "core/internal/mediums/ble_advertisement_header.h"

#include "platform/base64_utils.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

const BLEAdvertisementHeader::Version::Value kVersion =
    BLEAdvertisementHeader::Version::V2;
const std::uint32_t kNumSlots = 2;
const char kServiceIDBloomFilter[] = {0x01, 0x02, 0x03, 0x04, 0x05,
                                      0x06, 0x07, 0x08, 0x09, 0x0A};
const char kAdvertisementHash[] = {0x0A, 0x0B, 0x0C, 0x0D};
const size_t kAdvertisementHeaderLength = 15;
const size_t kLongAdvertisementHeaderLength = kAdvertisementHeaderLength + 1;
const size_t kShortAdvertisementHeaderLength = kAdvertisementHeaderLength - 1;

TEST(BLEAdvertisementHeader, SerializationDeserializationWorks) {
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_bloom_filter(MakeConstPtr(
      new ByteArray(kServiceIDBloomFilter,
                    sizeof(kServiceIDBloomFilter) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_advertisement_hash(
      MakeConstPtr(new ByteArray(kAdvertisementHash,
                                 sizeof(kAdvertisementHash) / sizeof(char))));

  std::string ble_advertisement_header_string(BLEAdvertisementHeader::asString(
      kVersion, kNumSlots, scoped_service_id_bloom_filter.get(),
      scoped_advertisement_hash.get()));
  ScopedPtr<ConstPtr<BLEAdvertisementHeader> > scoped_ble_advertisement_header(
      BLEAdvertisementHeader::fromString(ble_advertisement_header_string));

  ASSERT_EQ(kVersion, scoped_ble_advertisement_header->getVersion());
  ASSERT_EQ(kNumSlots, scoped_ble_advertisement_header->getNumSlots());
  ASSERT_EQ(
      0,
      memcmp(
          kServiceIDBloomFilter,
          scoped_ble_advertisement_header->getServiceIdBloomFilter()->getData(),
          scoped_ble_advertisement_header->getServiceIdBloomFilter()->size()));
  ASSERT_EQ(
      0,
      memcmp(kAdvertisementHash,
             scoped_ble_advertisement_header->getAdvertisementHash()->getData(),
             scoped_ble_advertisement_header->getAdvertisementHash()->size()));
}

TEST(BLEAdvertisementHeader, SerializationFailsWithBadVersion) {
  BLEAdvertisementHeader::Version::Value bad_version =
      static_cast<BLEAdvertisementHeader::Version::Value>(666);

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_bloom_filter(MakeConstPtr(
      new ByteArray(kServiceIDBloomFilter,
                    sizeof(kServiceIDBloomFilter) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_advertisement_hash(
      MakeConstPtr(new ByteArray(kAdvertisementHash,
                                 sizeof(kAdvertisementHash) / sizeof(char))));

  std::string ble_advertisement_header_string(BLEAdvertisementHeader::asString(
      bad_version, kNumSlots, scoped_service_id_bloom_filter.get(),
      scoped_advertisement_hash.get()));

  ASSERT_EQ("", ble_advertisement_header_string);
}

TEST(BLEAdvertisementHeader, SerializationFailsWithShortServiceIdBloomFilter) {
  char short_service_id_bloom_filter[] = {0x01, 0x02, 0x03, 0x04, 0x05,
                                          0x06, 0x07, 0x08, 0x09};

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_bloom_filter(MakeConstPtr(
      new ByteArray(short_service_id_bloom_filter,
                    sizeof(short_service_id_bloom_filter) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_advertisement_hash(
      MakeConstPtr(new ByteArray(kAdvertisementHash,
                                 sizeof(kAdvertisementHash) / sizeof(char))));

  std::string ble_advertisement_header_string(BLEAdvertisementHeader::asString(
      kVersion, kNumSlots, scoped_service_id_bloom_filter.get(),
      scoped_advertisement_hash.get()));

  ASSERT_EQ("", ble_advertisement_header_string);
}

TEST(BLEAdvertisementHeader, SerializationFailsWithLongServiceIdBloomFilter) {
  char long_service_id_bloom_filter[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                         0x07, 0x08, 0x09, 0x0A, 0x0B};

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_bloom_filter(MakeConstPtr(
      new ByteArray(long_service_id_bloom_filter,
                    sizeof(long_service_id_bloom_filter) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_advertisement_hash(
      MakeConstPtr(new ByteArray(kAdvertisementHash,
                                 sizeof(kAdvertisementHash) / sizeof(char))));

  std::string ble_advertisement_header_string(BLEAdvertisementHeader::asString(
      kVersion, kNumSlots, scoped_service_id_bloom_filter.get(),
      scoped_advertisement_hash.get()));

  ASSERT_EQ("", ble_advertisement_header_string);
}

TEST(BLEAdvertisementHeader, SerializationFailsWithShortAdvertisementHash) {
  char short_advertisement_hash[] = {0x0A, 0x0B, 0x0C};

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_bloom_filter(MakeConstPtr(
      new ByteArray(kServiceIDBloomFilter,
                    sizeof(kServiceIDBloomFilter) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_advertisement_hash(MakeConstPtr(
      new ByteArray(short_advertisement_hash,
                    sizeof(short_advertisement_hash) / sizeof(char))));

  std::string ble_advertisement_header_string(BLEAdvertisementHeader::asString(
      kVersion, kNumSlots, scoped_service_id_bloom_filter.get(),
      scoped_advertisement_hash.get()));

  ASSERT_EQ("", ble_advertisement_header_string);
}

TEST(BLEAdvertisementHeader, SerializationFailsWithLongAdvertisementHash) {
  char long_advertisement_hash[] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E};

  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_bloom_filter(MakeConstPtr(
      new ByteArray(kServiceIDBloomFilter,
                    sizeof(kServiceIDBloomFilter) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_advertisement_hash(MakeConstPtr(
      new ByteArray(long_advertisement_hash,
                    sizeof(long_advertisement_hash) / sizeof(char))));

  std::string ble_advertisement_header_string(BLEAdvertisementHeader::asString(
      kVersion, kNumSlots, scoped_service_id_bloom_filter.get(),
      scoped_advertisement_hash.get()));

  ASSERT_EQ("", ble_advertisement_header_string);
}

TEST(BLEAdvertisementHeader, DeserializationWorksWithExtraBytes) {
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_bloom_filter(MakeConstPtr(
      new ByteArray(kServiceIDBloomFilter,
                    sizeof(kServiceIDBloomFilter) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_advertisement_hash(
      MakeConstPtr(new ByteArray(kAdvertisementHash,
                                 sizeof(kAdvertisementHash) / sizeof(char))));
  std::string ble_advertisement_header_string =
      BLEAdvertisementHeader::asString(kVersion, kNumSlots,
                                       scoped_service_id_bloom_filter.get(),
                                       scoped_advertisement_hash.get());

  // Base64 decode the string, add a character, and then re-encode it. We must
  // explicitly define how long our array is because we can't use variable
  // length arrays.
  ScopedPtr<Ptr<ByteArray> > scoped_ble_advertisement_header_bytes(
      Base64Utils::decode(ble_advertisement_header_string));
  char raw_long_ble_advertisement_header_bytes[kLongAdvertisementHeaderLength];
  memcpy(raw_long_ble_advertisement_header_bytes,
         scoped_ble_advertisement_header_bytes->getData(),
         kLongAdvertisementHeaderLength);
  ScopedPtr<ConstPtr<ByteArray> > scoped_long_ble_advertisement_header_bytes(
      MakeConstPtr(new ByteArray(raw_long_ble_advertisement_header_bytes,
                                 kLongAdvertisementHeaderLength)));
  std::string long_ble_advertisement_header_string =
      Base64Utils::encode(scoped_long_ble_advertisement_header_bytes.get());

  ScopedPtr<ConstPtr<BLEAdvertisementHeader> > scoped_ble_advertisement_header(
      BLEAdvertisementHeader::fromString(long_ble_advertisement_header_string));

  ASSERT_EQ(kVersion, scoped_ble_advertisement_header->getVersion());
  ASSERT_EQ(kNumSlots, scoped_ble_advertisement_header->getNumSlots());
  ASSERT_EQ(
      0,
      memcmp(
          kServiceIDBloomFilter,
          scoped_ble_advertisement_header->getServiceIdBloomFilter()->getData(),
          scoped_ble_advertisement_header->getServiceIdBloomFilter()->size()));
  ASSERT_EQ(
      0,
      memcmp(kAdvertisementHash,
             scoped_ble_advertisement_header->getAdvertisementHash()->getData(),
             scoped_ble_advertisement_header->getAdvertisementHash()->size()));
}

TEST(BLEAdvertisementHeader, DeserializationFailsWithShortLength) {
  ScopedPtr<ConstPtr<ByteArray> > scoped_service_id_bloom_filter(MakeConstPtr(
      new ByteArray(kServiceIDBloomFilter,
                    sizeof(kServiceIDBloomFilter) / sizeof(char))));
  ScopedPtr<ConstPtr<ByteArray> > scoped_advertisement_hash(
      MakeConstPtr(new ByteArray(kAdvertisementHash,
                                 sizeof(kAdvertisementHash) / sizeof(char))));
  std::string ble_advertisement_header_string =
      BLEAdvertisementHeader::asString(kVersion, kNumSlots,
                                       scoped_service_id_bloom_filter.get(),
                                       scoped_advertisement_hash.get());

  // Base64 decode the string, remove a character, and then re-encode it. We
  // must explicitly define how long our array is because we can't use variable
  // length arrays.
  ScopedPtr<Ptr<ByteArray> > scoped_ble_advertisement_header_bytes(
      Base64Utils::decode(ble_advertisement_header_string));
  char
      raw_short_ble_advertisement_header_bytes[kShortAdvertisementHeaderLength];
  memcpy(raw_short_ble_advertisement_header_bytes,
         scoped_ble_advertisement_header_bytes->getData(),
         kShortAdvertisementHeaderLength);
  ScopedPtr<ConstPtr<ByteArray> > scoped_short_ble_advertisement_header_bytes(
      MakeConstPtr(new ByteArray(raw_short_ble_advertisement_header_bytes,
                                 kShortAdvertisementHeaderLength)));
  std::string short_ble_advertisement_header_string =
      Base64Utils::encode(scoped_short_ble_advertisement_header_bytes.get());

  ScopedPtr<ConstPtr<BLEAdvertisementHeader> > scoped_ble_advertisement_header(
      BLEAdvertisementHeader::fromString(
          short_ble_advertisement_header_string));

  ASSERT_TRUE(scoped_ble_advertisement_header.isNull());
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
