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

#include "connections/implementation/mediums/ble_v2/ble_advertisement_header.h"

#include <string>

#include "gtest/gtest.h"
#include "absl/hash/hash_testing.h"
#include "internal/platform/base64_utils.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {

constexpr BleAdvertisementHeader::Version kVersion =
    BleAdvertisementHeader::Version::kV2;
constexpr int kNumSlots = 2;
constexpr int kPsmValue = 127;
constexpr absl::string_view kServiceIDBloomFilter{
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a"};
constexpr absl::string_view kAdvertisementHash{"\x0a\x0b\x0c\x0d"};

TEST(BleAdvertisementHeaderTest, ConstructionWorks) {
  ByteArray service_id_bloom_filter((std::string(kServiceIDBloomFilter)));
  ByteArray advertisement_hash((std::string(kAdvertisementHash)));

  BleAdvertisementHeader ble_advertisement_header(
      kVersion, false, kNumSlots, service_id_bloom_filter, advertisement_hash,
      kPsmValue);

  EXPECT_TRUE(ble_advertisement_header.IsValid());
  EXPECT_EQ(kVersion, ble_advertisement_header.GetVersion());
  EXPECT_FALSE(ble_advertisement_header.IsSupportExtendedAdvertisement());
  EXPECT_EQ(kNumSlots, ble_advertisement_header.GetNumSlots());
  EXPECT_EQ(service_id_bloom_filter,
            ble_advertisement_header.GetServiceIdBloomFilter());
  EXPECT_EQ(advertisement_hash,
            ble_advertisement_header.GetAdvertisementHash());
  EXPECT_EQ(kPsmValue, ble_advertisement_header.GetPsm());
}

TEST(BleAdvertisementHeaderTest, ConstructionFailsWithBadVersion) {
  auto bad_version = static_cast<BleAdvertisementHeader::Version>(666);

  ByteArray service_id_bloom_filter((std::string(kServiceIDBloomFilter)));
  ByteArray advertisement_hash((std::string(kAdvertisementHash)));
  BleAdvertisementHeader ble_advertisement_header(
      bad_version, false, kNumSlots, service_id_bloom_filter,
      advertisement_hash, kPsmValue);

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest, ConstructionSucceedsWithZeroNumSlot) {
  int num_slot = 0;

  ByteArray service_id_bloom_filter((std::string(kServiceIDBloomFilter)));
  ByteArray advertisement_hash((std::string(kAdvertisementHash)));

  BleAdvertisementHeader ble_advertisement_header(
      kVersion, false, num_slot, service_id_bloom_filter, advertisement_hash,
      kPsmValue);

  EXPECT_TRUE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest, ConstructionFailsWithNegativeNumSlot) {
  int num_slot = -1;

  ByteArray service_id_bloom_filter((std::string(kServiceIDBloomFilter)));
  ByteArray advertisement_hash((std::string(kAdvertisementHash)));

  BleAdvertisementHeader ble_advertisement_header(
      kVersion, false, num_slot, service_id_bloom_filter, advertisement_hash,
      kPsmValue);

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest,
     ConstructionFailsWithShortServiceIdBloomFilter) {
  char short_service_id_bloom_filter[] = "\x01\x02\x03\x04\x05\x06\x07\x08\x09";

  ByteArray short_service_id_bloom_filter_bytes(short_service_id_bloom_filter);
  ByteArray advertisement_hash((std::string(kAdvertisementHash)));

  BleAdvertisementHeader ble_advertisement_header(
      kVersion, false, kNumSlots, short_service_id_bloom_filter_bytes,
      advertisement_hash, kPsmValue);

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest,
     ConstructionFailsWithLongServiceIdBloomFilter) {
  char long_service_id_bloom_filter[] =
      "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b";

  ByteArray service_id_bloom_filter{long_service_id_bloom_filter};
  ByteArray advertisement_hash((std::string(kAdvertisementHash)));

  BleAdvertisementHeader ble_advertisement_header(
      kVersion, false, kNumSlots, service_id_bloom_filter, advertisement_hash,
      kPsmValue);

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest, ConstructionFailsWithShortAdvertisementHash) {
  char short_advertisement_hash[] = "\x0a\x0b\x0c";

  ByteArray service_id_bloom_filter((std::string(kServiceIDBloomFilter)));
  ByteArray advertisement_hash(short_advertisement_hash);

  BleAdvertisementHeader ble_advertisement_header(
      kVersion, false, kNumSlots, service_id_bloom_filter, advertisement_hash,
      kPsmValue);

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest, ConstructionFailsWithLongAdvertisementHash) {
  char long_advertisement_hash[] = "\x0a\x0b\x0c\x0d\x0e";

  ByteArray service_id_bloom_filter((std::string(kServiceIDBloomFilter)));
  ByteArray advertisement_hash(long_advertisement_hash);
  BleAdvertisementHeader ble_advertisement_header{
      kVersion,           false,    kNumSlots, service_id_bloom_filter,
      advertisement_hash, kPsmValue};

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest, ConstructionFromSerializedStringWorks) {
  ByteArray service_id_bloom_filter((std::string(kServiceIDBloomFilter)));
  ByteArray advertisement_hash((std::string(kAdvertisementHash)));

  BleAdvertisementHeader org_ble_advertisement_header(
      kVersion, false, kNumSlots, service_id_bloom_filter, advertisement_hash,
      kPsmValue);
  auto ble_advertisement_header_bytes = ByteArray(org_ble_advertisement_header);

  BleAdvertisementHeader ble_advertisement_header(
      ble_advertisement_header_bytes);

  EXPECT_TRUE(ble_advertisement_header.IsValid());
  EXPECT_EQ(kVersion, ble_advertisement_header.GetVersion());
  EXPECT_FALSE(ble_advertisement_header.IsSupportExtendedAdvertisement());
  EXPECT_EQ(kNumSlots, ble_advertisement_header.GetNumSlots());
  EXPECT_EQ(service_id_bloom_filter,
            ble_advertisement_header.GetServiceIdBloomFilter());
  EXPECT_EQ(advertisement_hash,
            ble_advertisement_header.GetAdvertisementHash());
  EXPECT_EQ(kPsmValue, ble_advertisement_header.GetPsm());
}

TEST(BleAdvertisementHeaderTest, ConstructionFromDecodedByteArrayFails) {
  ByteArray service_id_bloom_filter((std::string(kServiceIDBloomFilter)));
  ByteArray advertisement_hash((std::string(kAdvertisementHash)));

  BleAdvertisementHeader ble_advertisement_header(
      kVersion, false, kNumSlots, service_id_bloom_filter, advertisement_hash,
      kPsmValue);
  auto ble_advertisement_header_bytes = ByteArray(ble_advertisement_header);

  // Decode the returned byte array and input as a parameter by construction.
  BleAdvertisementHeader failed_ble_advertisement_header(
      Base64Utils::Decode(ble_advertisement_header_bytes.AsStringView()));

  EXPECT_FALSE(failed_ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest, Hash) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      BleAdvertisementHeader(),
      BleAdvertisementHeader(kVersion, false, 0,
                             ByteArray(std::string(kServiceIDBloomFilter)),
                             ByteArray(std::string(kAdvertisementHash)), 0),
      BleAdvertisementHeader(
          kVersion, true, 2,
          ByteArray("\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15"),
          ByteArray("\x0E\x0F\x0G\x0H"), 1),
      BleAdvertisementHeader(
          kVersion, false, 4,
          ByteArray("\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29"),
          ByteArray("\x0i\x0j\x0k\x0l"), 2),
  }));
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
