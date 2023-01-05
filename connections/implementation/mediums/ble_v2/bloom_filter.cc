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

#include "absl/numeric/int128.h"
#include "absl/strings/numbers.h"
#include "internal/platform/logging.h"
#include "src/MurmurHash3.h"

namespace nearby {
namespace connections {
namespace mediums {

namespace {
constexpr int kHasherNumberOfRepetitions = 5;
}

BloomFilter::BloomFilter(std::unique_ptr<BitSet> bit_set,
                         const ByteArray& bytes)
    : bit_set_(std::move(bit_set)) {
  const char* bytes_read_ptr = bytes.data();

  if (bytes.size() == 0) {
    // Ignore it; we don't need to copy the bit for the empty bytes.
    return;
  }
  // If the size is not matched, fall out.
  if (bytes.size() * 8 != bit_set_->Size()) {
    NEARBY_LOGS(INFO) << "Cannot construct from bytes since the size is not "
                         "matched. bytes.size(x8) = "
                      << bytes.size() << ", bit_set.size=" << bit_set_->Size();
    return;
  }
  for (size_t byte_index = 0; byte_index < bytes.size(); byte_index++) {
    for (size_t bit_index = 0; bit_index < 8; bit_index++) {
      bit_set_->Set((byte_index * 8) + bit_index,
                    (*bytes_read_ptr >> bit_index) & 0x01);
    }
    bytes_read_ptr++;
  }
}

BloomFilter::operator ByteArray() const {
  std::string bitset_binary_string = bit_set_->ToString();

  ByteArray result_bytes(GetMinBytesForBits());
  char* result_bytes_write_ptr = result_bytes.data();
  // We go through the string backwards because the rightmost character
  // corresponds to position 0 in the bitset.
  for (size_t i = bit_set_->Size(); i > 0; i -= 8) {
    std::string byte_binary_string = bitset_binary_string.substr(i - 8, 8);
    std::uint32_t byte_value;
    absl::numbers_internal::safe_strtou32_base(byte_binary_string, &byte_value,
                                               /* base= */ 2);
    *result_bytes_write_ptr = static_cast<char>(byte_value & 0x000000FF);
    result_bytes_write_ptr++;
  }
  return result_bytes;
}

void BloomFilter::Add(const std::string& s) {
  std::vector<std::int32_t> hashes = GetHashes(s);
  for (int32_t hash : hashes) {
    size_t position = static_cast<size_t>(hash) % bit_set_->Size();
    bit_set_->Set(position, true);
  }
}

bool BloomFilter::PossiblyContains(const std::string& s) {
  std::vector<std::int32_t> hashes = GetHashes(s);
  for (int32_t hash : hashes) {
    size_t position = static_cast<size_t>(hash) % bit_set_->Size();
    if (!bit_set_->Test(position)) {
      return false;
    }
  }
  return true;
}

std::vector<std::int32_t> BloomFilter::GetHashes(const std::string& s) {
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
