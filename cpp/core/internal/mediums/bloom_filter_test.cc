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

#include "core/internal/mediums/bloom_filter.h"

#include <algorithm>

#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {
namespace {

const size_t kByteArrayLength = 100;

TEST(BloomFilterTest, EmptyFilterReturnsEmptyArray) {
  ScopedPtr<Ptr<BloomFilter<kByteArrayLength>>> scoped_bloom_filter(
      new BloomFilter<kByteArrayLength>());

  ScopedPtr<ConstPtr<ByteArray>> scoped_bloom_filter_bytes(
      scoped_bloom_filter->asBytes());
  std::string empty_string(kByteArrayLength, '\0');
  ASSERT_EQ(0, memcmp(scoped_bloom_filter_bytes->getData(), empty_string.data(),
                      empty_string.size()));
}

TEST(BloomFilterTest, EmptyFilterNeverContains) {
  ScopedPtr<Ptr<BloomFilter<kByteArrayLength>>> scoped_bloom_filter(
      new BloomFilter<kByteArrayLength>());

  ASSERT_FALSE(scoped_bloom_filter->possiblyContains("ELEMENT_1"));
  ASSERT_FALSE(scoped_bloom_filter->possiblyContains("ELEMENT_2"));
  ASSERT_FALSE(scoped_bloom_filter->possiblyContains("ELEMENT_3"));
}

TEST(BloomFilterTest, AddSuccess) {
  ScopedPtr<Ptr<BloomFilter<kByteArrayLength>>> scoped_bloom_filter(
      new BloomFilter<kByteArrayLength>());
  ASSERT_FALSE(scoped_bloom_filter->possiblyContains("ELEMENT_1"));

  scoped_bloom_filter->add("ELEMENT_1");
  ASSERT_TRUE(scoped_bloom_filter->possiblyContains("ELEMENT_1"));
}

TEST(BloomFilterTest, AddOnlyGivenArg) {
  ScopedPtr<Ptr<BloomFilter<kByteArrayLength>>> scoped_bloom_filter(
      new BloomFilter<kByteArrayLength>());
  scoped_bloom_filter->add("ELEMENT_1");

  ASSERT_TRUE(scoped_bloom_filter->possiblyContains("ELEMENT_1"));
  ASSERT_FALSE(scoped_bloom_filter->possiblyContains("ELEMENT_2"));
  ASSERT_FALSE(scoped_bloom_filter->possiblyContains("ELEMENT_3"));
}

TEST(BloomFilterTest, AddMultipleArgs) {
  ScopedPtr<Ptr<BloomFilter<kByteArrayLength>>> scoped_bloom_filter(
      new BloomFilter<kByteArrayLength>());
  scoped_bloom_filter->add("ELEMENT_1");
  scoped_bloom_filter->add("ELEMENT_2");

  ASSERT_TRUE(scoped_bloom_filter->possiblyContains("ELEMENT_1"));
  ASSERT_TRUE(scoped_bloom_filter->possiblyContains("ELEMENT_2"));
  ASSERT_FALSE(scoped_bloom_filter->possiblyContains("ELEMENT_3"));
}

TEST(BloomFilterTest, AddMultipleArgsReturnsNonemptyArray) {
  ScopedPtr<Ptr<BloomFilter<10>>> scoped_bloom_filter(new BloomFilter<10>());
  scoped_bloom_filter->add("ELEMENT_1");
  scoped_bloom_filter->add("ELEMENT_2");
  scoped_bloom_filter->add("ELEMENT_3");

  ScopedPtr<ConstPtr<ByteArray>> scoped_bloom_filter_bytes(
      scoped_bloom_filter->asBytes());
  std::string empty_string(kByteArrayLength, '\0');
  ASSERT_NE(scoped_bloom_filter_bytes->asString(), empty_string);
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
  ScopedPtr<Ptr<BloomFilter<kByteArrayLength>>> scoped_bloom_filter(
      new BloomFilter<kByteArrayLength>());
  // Add one element to our BloomFilter.
  scoped_bloom_filter->add("ELEMENT_1");

  std::int32_t non_zero_count = 0;
  std::int32_t longest_zero_streak = 0;
  std::int32_t current_zero_streak = 0;

  // Record the amount of non-zero bytes and the longest streak of zero bytes in
  // the resulting BloomFilter. This is an approximation of reasonable
  // distribution since we're recording by bytes instead of bits.
  ScopedPtr<ConstPtr<ByteArray>> scoped_bloom_filter_bytes(
      scoped_bloom_filter->asBytes());
  const char* bloom_filter_bytes_read_ptr =
      scoped_bloom_filter_bytes->getData();
  for (int i = 0; i < scoped_bloom_filter_bytes->size(); i++) {
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
  ASSERT_TRUE(longest_zero_streak <= longest_acceptable_zero_streak);
}

TEST(BloomFilterTest, RandomnessFalsePositiveRate) {
  ScopedPtr<Ptr<BloomFilter<10>>> scoped_bloom_filter(new BloomFilter<10>());
  // Add 5 distinct elements to the BloomFilter.
  scoped_bloom_filter->add("ELEMENT_1");
  scoped_bloom_filter->add("ELEMENT_2");
  scoped_bloom_filter->add("ELEMENT_3");
  scoped_bloom_filter->add("ELEMENT_4");
  scoped_bloom_filter->add("ELEMENT_5");

  std::int32_t false_positives = 0;
  // Now test 100 other elements and record the number of false positives.
  for (int i = 5; i < 105; i++) {
    false_positives +=
        scoped_bloom_filter->possiblyContains("ELEMENT_" + std::to_string(i))
            ? 1
            : 0;
  }

  // We expect the false positive rate to be 3% with 5 elements in a 10 byte
  // filter. Thus, we give a little leeway and verify that the false positive
  // rate is no more than 5%.
  ASSERT_LE(false_positives, 5);
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
