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

#include "absl/numeric/int128.h"
#include "absl/strings/numbers.h"
#include "smhasher/src/MurmurHash3.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

template <size_t CapacityInBytes>
const std::int32_t BloomFilter<CapacityInBytes>::kHasherNumberOfRepetitions = 5;

template <size_t CapacityInBytes>
BloomFilter<CapacityInBytes>::BloomFilter() : bits_() {}

template <size_t CapacityInBytes>
BloomFilter<CapacityInBytes>::BloomFilter(ConstPtr<ByteArray> bytes) : bits_() {
  const char* bytes_read_ptr = bytes->getData();
  for (size_t byte_index = 0; byte_index < bytes->size(); byte_index++) {
    for (size_t bit_index = 0; bit_index < 8; bit_index++) {
      bits_.set((byte_index * 8) + bit_index,
                (*bytes_read_ptr >> bit_index) & 0x01);
    }
    bytes_read_ptr++;
  }
}

template <size_t CapacityInBytes>
BloomFilter<CapacityInBytes>::~BloomFilter() {
  // Nothing to do.
}

template <size_t CapacityInBytes>
ConstPtr<ByteArray> BloomFilter<CapacityInBytes>::asBytes() {
  // Gets a binary string representation of the bitset where the leftmost
  // character corresponds to bitset position (total size) - 1.
  //
  // If the bitset's internal representation is:
  //     [position 0]     0 0 1 1 0 0 0 1 0 1 0 1     [position 11]
  // The string representation will be outputted like this:
  //                     "1 0 1 0 1 0 0 0 1 1 0 0"
  std::string bitset_binary_string = bits_.to_string();

  Ptr<ByteArray> result_bytes{new ByteArray{CapacityInBytes}};
  char* result_bytes_write_ptr = result_bytes->getData();
  // We go through the string backwards because the rightmost character
  // corresponds to position 0 in the bitset.
  for (size_t i = bits_.size(); i > 0; i -= 8) {
    std::string byte_binary_string = bitset_binary_string.substr(i - 8, 8);
    std::uint32_t byte_value;
    absl::numbers_internal::safe_strtou32_base(byte_binary_string, &byte_value,
                                               /* base= */ 2);
    *result_bytes_write_ptr = static_cast<char>(byte_value & 0x000000FF);
    result_bytes_write_ptr++;
  }
  return ConstifyPtr(result_bytes);
}

template <size_t CapacityInBytes>
void BloomFilter<CapacityInBytes>::add(const std::string& s) {
  std::vector<std::int32_t> hashes = getHashes(s);
  for (std::vector<std::int32_t>::iterator it = hashes.begin();
       it != hashes.end(); ++it) {
    size_t position = static_cast<size_t>(*it) % bits_.size();
    bits_.set(position);
  }
}

template <size_t CapacityInBytes>
bool BloomFilter<CapacityInBytes>::possiblyContains(const std::string& s) {
  std::vector<std::int32_t> hashes = getHashes(s);
  for (std::vector<std::int32_t>::iterator i = hashes.begin();
       i != hashes.end(); ++i) {
    size_t position = static_cast<size_t>(*i) % bits_.size();
    if (!bits_.test(position)) {
      return false;
    }
  }
  return true;
}

template <size_t CapacityInBytes>
std::vector<std::int32_t> BloomFilter<CapacityInBytes>::getHashes(
    const std::string& s) {
  std::vector<std::int32_t> hashes(kHasherNumberOfRepetitions, 0);

  absl::uint128 hash128;
  MurmurHash3_x64_128(s.data(), s.size(), 0, &hash128);
  std::uint64_t hash64 =
      absl::Uint128Low64(hash128);  // the lower 64 bits of the 128-bit hash
  std::int32_t hash1 = static_cast<std::int32_t>(
      hash64 & 0x00000000FFFFFFFF);  // the lower 32 bits of the 64-bit hash
  std::int32_t hash2 = static_cast<std::int32_t>(
      (hash64 >> 32) & 0x0FFFFFFFF);  // the upper 32 bits of the 64-bit hash
  for (size_t i = 1; i <= kHasherNumberOfRepetitions; i++) {
    std::int32_t combinedHash = static_cast<std::int32_t>(hash1 + (i * hash2));
    // Flip all the bits if it's negative (guaranteed positive number)
    if (combinedHash < 0) combinedHash = ~combinedHash;
    hashes[i - 1] = combinedHash;
  }
  return hashes;
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
