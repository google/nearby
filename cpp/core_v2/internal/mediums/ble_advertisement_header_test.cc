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

#include "core_v2/internal/mediums/ble_advertisement_header.h"

#include "platform_v2/base/base64_utils.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {
namespace {

constexpr BleAdvertisementHeader::Version kVersion =
    BleAdvertisementHeader::Version::kV2;
constexpr int kNumSlots = 2;
constexpr absl::string_view kServiceIDBloomFilter{
    "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a"};
constexpr absl::string_view kAdvertisementHash{"\x0a\x0b\x0c\x0d"};

TEST(BleAdvertisementHeaderTest, ConstructionWorks) {
  ByteArray service_id_bloom_filter{std::string(kServiceIDBloomFilter)};
  ByteArray advertisement_hash{std::string(kAdvertisementHash)};

  BleAdvertisementHeader ble_advertisement_header{
      kVersion, kNumSlots, service_id_bloom_filter, advertisement_hash};

  EXPECT_TRUE(ble_advertisement_header.IsValid());
  EXPECT_EQ(kVersion, ble_advertisement_header.GetVersion());
  EXPECT_EQ(kNumSlots, ble_advertisement_header.GetNumSlots());
  EXPECT_EQ(service_id_bloom_filter,
            ble_advertisement_header.GetServiceIdBloomFilter());
  EXPECT_EQ(advertisement_hash,
            ble_advertisement_header.GetAdvertisementHash());
}

TEST(BleAdvertisementHeaderTest, ConstructionFailsWithBadVersion) {
  auto bad_version = static_cast<BleAdvertisementHeader::Version>(666);

  ByteArray service_id_bloom_filter{std::string(kServiceIDBloomFilter)};
  ByteArray advertisement_hash{std::string(kAdvertisementHash)};

  BleAdvertisementHeader ble_advertisement_header{
      bad_version, kNumSlots, service_id_bloom_filter, advertisement_hash};

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest, ConstructionFailsWitZeroNumSlot) {
  int num_slot = 0;

  ByteArray service_id_bloom_filter{std::string(kServiceIDBloomFilter)};
  ByteArray advertisement_hash{std::string(kAdvertisementHash)};

  BleAdvertisementHeader ble_advertisement_header{
      kVersion, num_slot, service_id_bloom_filter, advertisement_hash};

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest,
     ConstructionFailsWithShortServiceIdBloomFilter) {
  char short_service_id_bloom_filter[] = "\x01\x02\x03\x04\x05\x06\x07\x08\x09";

  ByteArray short_service_id_bloom_filter_bytes{short_service_id_bloom_filter};
  ByteArray advertisement_hash{std::string(kAdvertisementHash)};

  BleAdvertisementHeader ble_advertisement_header{
      kVersion, kNumSlots, short_service_id_bloom_filter_bytes,
      advertisement_hash};

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest,
     ConstructionFailsWithLongServiceIdBloomFilter) {
  char long_service_id_bloom_filter[] =
      "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b";

  ByteArray service_id_bloom_filter{long_service_id_bloom_filter};
  ByteArray advertisement_hash{std::string(kAdvertisementHash)};

  BleAdvertisementHeader ble_advertisement_header{
      kVersion, kNumSlots, service_id_bloom_filter, advertisement_hash};

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest, ConstructionFailsWithShortAdvertisementHash) {
  char short_advertisement_hash[] = "\x0a\x0b\x0c";

  ByteArray service_id_bloom_filter{std::string(kServiceIDBloomFilter)};
  ByteArray advertisement_hash{short_advertisement_hash};

  BleAdvertisementHeader ble_advertisement_header{
      kVersion, kNumSlots, service_id_bloom_filter, advertisement_hash};

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest, ConstructionFailsWithLongAdvertisementHash) {
  char long_advertisement_hash[] = "\x0a\x0b\x0c\x0d\x0e";

  ByteArray service_id_bloom_filter{std::string(kServiceIDBloomFilter)};
  ByteArray advertisement_hash{long_advertisement_hash};
  BleAdvertisementHeader ble_advertisement_header{
      kVersion, kNumSlots, service_id_bloom_filter, advertisement_hash};

  EXPECT_FALSE(ble_advertisement_header.IsValid());
}

TEST(BleAdvertisementHeaderTest, ConstructionFromSerializedStringWorks) {
  ByteArray service_id_bloom_filter{std::string(kServiceIDBloomFilter)};
  ByteArray advertisement_hash{std::string(kAdvertisementHash)};

  BleAdvertisementHeader org_ble_advertisement_header{
      kVersion, kNumSlots, service_id_bloom_filter, advertisement_hash};
  auto ble_advertisement_header_string =
      std::string(org_ble_advertisement_header);

  BleAdvertisementHeader ble_advertisement_header{
      ble_advertisement_header_string};

  EXPECT_TRUE(ble_advertisement_header.IsValid());
  EXPECT_EQ(kVersion, ble_advertisement_header.GetVersion());
  EXPECT_EQ(kNumSlots, ble_advertisement_header.GetNumSlots());
  EXPECT_EQ(service_id_bloom_filter,
            ble_advertisement_header.GetServiceIdBloomFilter());
  EXPECT_EQ(advertisement_hash,
            ble_advertisement_header.GetAdvertisementHash());
}

TEST(BleAdvertisementHeaderTest, ConstructionFromExtraBytesWorks) {
  ByteArray service_id_bloom_filter{std::string(kServiceIDBloomFilter)};
  ByteArray advertisement_hash{std::string(kAdvertisementHash)};

  BleAdvertisementHeader ble_advertisement_header{
      kVersion, kNumSlots, service_id_bloom_filter, advertisement_hash};
  auto ble_advertisement_header_string = std::string(ble_advertisement_header);

  // Base64 decode the string, add a character, and then re-encode it.
  ByteArray ble_advertisement_header_bytes =
      Base64Utils::Decode(ble_advertisement_header_string);
  ByteArray long_ble_advertisement_header_bytes{
      ble_advertisement_header_bytes.size() + 1};
  long_ble_advertisement_header_bytes.CopyAt(0, ble_advertisement_header_bytes);
  std::string long_ble_advertisement_header_string{
      Base64Utils::Encode(long_ble_advertisement_header_bytes)};

  BleAdvertisementHeader long_ble_advertisement_header{
      long_ble_advertisement_header_string};

  EXPECT_TRUE(long_ble_advertisement_header.IsValid());
  EXPECT_EQ(kVersion, long_ble_advertisement_header.GetVersion());
  EXPECT_EQ(kNumSlots, long_ble_advertisement_header.GetNumSlots());
  EXPECT_EQ(service_id_bloom_filter,
            long_ble_advertisement_header.GetServiceIdBloomFilter());
  EXPECT_EQ(advertisement_hash,
            long_ble_advertisement_header.GetAdvertisementHash());
}

TEST(BleAdvertisementHeaderTest, ConstructionFromShortLengthFails) {
  ByteArray service_id_bloom_filter{std::string(kServiceIDBloomFilter)};
  ByteArray advertisement_hash{std::string(kAdvertisementHash)};

  BleAdvertisementHeader ble_advertisement_header{
      kVersion, kNumSlots, service_id_bloom_filter, advertisement_hash};
  auto ble_advertisement_header_string = std::string(ble_advertisement_header);

  // Base64 decode the string, remove a character, and then re-encode it.
  ByteArray ble_advertisement_header_bytes =
      Base64Utils::Decode(ble_advertisement_header_string);
  ByteArray short_ble_advertisement_header_bytes{
      ble_advertisement_header_bytes.size() - 1};
  short_ble_advertisement_header_bytes.CopyAt(0,
                                              ble_advertisement_header_bytes);
  std::string short_ble_advertisement_header_string{
      Base64Utils::Encode(short_ble_advertisement_header_bytes)};

  BleAdvertisementHeader short_ble_advertisement_header{
      short_ble_advertisement_header_string};

  EXPECT_FALSE(short_ble_advertisement_header.IsValid());
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
