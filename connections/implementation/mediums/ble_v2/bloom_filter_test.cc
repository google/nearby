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

#include "connections/implementation/mediums/ble_v2/bloom_filter.h"

#include <algorithm>

#include "gtest/gtest.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace {

constexpr size_t kByteArrayLength = 100;

TEST(BloomFilterTest, EmptyFilterReturnsEmptyArray) {
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kByteArrayLength>>());

  ByteArray bloom_filter_bytes(bloom_filter);
  std::string empty_string(kByteArrayLength, '\0');

  EXPECT_EQ(empty_string, std::string(bloom_filter_bytes));
}

TEST(BloomFilterTest, EmptyFilterNeverContains) {
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kByteArrayLength>>());

  EXPECT_FALSE(bloom_filter.PossiblyContains("ELEMENT_1"));
  EXPECT_FALSE(bloom_filter.PossiblyContains("ELEMENT_2"));
  EXPECT_FALSE(bloom_filter.PossiblyContains("ELEMENT_3"));
}

TEST(BloomFilterTest, AddSuccess) {
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kByteArrayLength>>());

  EXPECT_FALSE(bloom_filter.PossiblyContains("ELEMENT_1"));

  bloom_filter.Add("ELEMENT_1");

  EXPECT_TRUE(bloom_filter.PossiblyContains("ELEMENT_1"));
}

TEST(BloomFilterTest, AddOnlyGivenArg) {
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kByteArrayLength>>());

  bloom_filter.Add("ELEMENT_1");

  EXPECT_TRUE(bloom_filter.PossiblyContains("ELEMENT_1"));
  EXPECT_FALSE(bloom_filter.PossiblyContains("ELEMENT_2"));
  EXPECT_FALSE(bloom_filter.PossiblyContains("ELEMENT_3"));
}

TEST(BloomFilterTest, AddMultipleArgs) {
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kByteArrayLength>>());

  bloom_filter.Add("ELEMENT_1");
  bloom_filter.Add("ELEMENT_2");

  EXPECT_TRUE(bloom_filter.PossiblyContains("ELEMENT_1"));
  EXPECT_TRUE(bloom_filter.PossiblyContains("ELEMENT_2"));
  EXPECT_FALSE(bloom_filter.PossiblyContains("ELEMENT_3"));
}

TEST(BloomFilterTest, AddMultipleArgsReturnsNonemptyArray) {
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<10>>());

  bloom_filter.Add("ELEMENT_1");
  bloom_filter.Add("ELEMENT_2");
  bloom_filter.Add("ELEMENT_3");

  ByteArray bloom_filter_bytes(bloom_filter);
  std::string empty_string(kByteArrayLength, '\0');

  EXPECT_NE(std::string(bloom_filter_bytes), empty_string);
}

TEST(BloomFilterTest, MoveConstructorSuccess) {
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kByteArrayLength>>());

  bloom_filter.Add("ELEMENT_1");

  BloomFilter bloom_filter_move{std::move(bloom_filter)};

  EXPECT_TRUE(bloom_filter_move.PossiblyContains("ELEMENT_1"));
}

TEST(BloomFilterTest, MoveAssignmentSuccess) {
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kByteArrayLength>>());

  bloom_filter.Add("ELEMENT_1");

  BloomFilter bloom_filter_move = std::move(bloom_filter);

  EXPECT_TRUE(bloom_filter_move.PossiblyContains("ELEMENT_1"));
}

/**
 * This test was added because of a bug where the BloomFilter doesn't utilize
 * all bits given. Functionally, the filter still works, but we just have a much
 * higher false positive rate. The bug was caused by confusing bit length and
 * byte length, which made our BloomFilter only set bits on the first byteLength
 * (bitLength / 8) bits rather than the whole bitLength bits.
 *
 * <p>Here, we're verifying that the bits set are somewhat scattered. So instead
 * of something like [ 0, 1, 1, 0, 0, 0, 0, ..., 0 ], we should be getting
 * something like [ 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, ..., 1, 0].
 */
TEST(BloomFilterTest, RandomnessNoEndBias) {
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kByteArrayLength>>());

  // Add one element to our BloomFilter.
  bloom_filter.Add("ELEMENT_1");

  std::int32_t non_zero_count = 0;
  std::int32_t longest_zero_streak = 0;
  std::int32_t current_zero_streak = 0;

  // Record the amount of non-zero bytes and the longest streak of zero bytes in
  // the resulting BloomFilter. This is an approximation of reasonable
  // distribution since we're recording by bytes instead of bits.
  ByteArray bloom_filter_bytes(bloom_filter);
  const char* bloom_filter_bytes_read_ptr = bloom_filter_bytes.data();
  for (int i = 0; i < bloom_filter_bytes.size(); i++) {
    if (*bloom_filter_bytes_read_ptr == '\0') {
      current_zero_streak++;
    } else {
      // Increment the number of non-zero bytes we've seen, update the longest
      // zero streak, and then reset the current zero streak.
      non_zero_count++;
      longest_zero_streak = std::max(longest_zero_streak, current_zero_streak);
      current_zero_streak = 0;
    }
    bloom_filter_bytes_read_ptr++;
  }
  // Update the longest zero streak again for the tail case.
  longest_zero_streak = std::min(longest_zero_streak, current_zero_streak);

  // Since randomness is hard to measure within one unit test, we instead do a
  // sanity check. All non-zero bytes should not be packed into one end of the
  // array.
  //
  // In this case, the size of one end is approximated to be:
  //     kByteArrayLength / nonZeroCount.
  // Therefore, the longest zero streak should be less than:
  //     kByteArrayLength - one end of the array.
  std::int32_t longest_acceptable_zero_streak =
      kByteArrayLength - (kByteArrayLength / non_zero_count);

  EXPECT_TRUE(longest_zero_streak <= longest_acceptable_zero_streak);
}

TEST(BloomFilterTest, RandomnessFalsePositiveRate) {
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kByteArrayLength>>());

  // Add 5 distinct elements to the BloomFilter.
  bloom_filter.Add("ELEMENT_1");
  bloom_filter.Add("ELEMENT_2");
  bloom_filter.Add("ELEMENT_3");
  bloom_filter.Add("ELEMENT_4");
  bloom_filter.Add("ELEMENT_5");

  std::int32_t false_positives = 0;
  // Now test 100 other elements and record the number of false positives.
  for (int i = 5; i < 105; i++) {
    false_positives +=
        bloom_filter.PossiblyContains("ELEMENT_" + std::to_string(i)) ? 1 : 0;
  }

  // We expect the false positive rate to be 3% with 5 elements in a 10 byte
  // filter. Thus, we give a little leeway and verify that the false positive
  // rate is no more than 5%.
  EXPECT_LE(false_positives, 5);
}

TEST(BloomFilterTest, ConstructWithNonEmptyByteArrayWorks) {
  BloomFilter bloom_filter(std::make_unique<BitSetImpl<kByteArrayLength>>());

  bloom_filter.Add("ELEMENT_1");
  ByteArray original_bloom_filter_bytes(bloom_filter);

  BloomFilter bloom_filter_inherited(
      std::make_unique<BitSetImpl<kByteArrayLength>>(),
      original_bloom_filter_bytes);

  EXPECT_TRUE(bloom_filter_inherited.PossiblyContains("ELEMENT_1"));
}

TEST(BloomFilterTest, ConstructLongByteArrayFails) {
  // Make 1 more byte in original BloomFilter.
  BloomFilter bloom_filter(
      std::make_unique<BitSetImpl<kByteArrayLength + 1>>());

  bloom_filter.Add("ELEMENT_1");
  ByteArray original_bloom_filter_bytes(bloom_filter);

  BloomFilter bloom_filter_inherited(
      std::make_unique<BitSetImpl<kByteArrayLength>>(),
      original_bloom_filter_bytes);

  EXPECT_FALSE(bloom_filter_inherited.PossiblyContains("ELEMENT_1"));
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
