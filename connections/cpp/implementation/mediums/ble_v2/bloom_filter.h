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

#ifndef CORE_INTERNAL_MEDIUMS_BLE_V2_BLOOM_FILTER_H_
#define CORE_INTERNAL_MEDIUMS_BLE_V2_BLOOM_FILTER_H_

#include <bitset>
#include <vector>

#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace mediums {

// Interface to set bits of the given bit array, by inserting a user element.
class BitSet {
 public:
  virtual ~BitSet() = default;

  // Gets a binary string representation of the bitset where the leftmost
  // character corresponds to bitset position (total size) - 1.
  //
  // If the bitset's internal representation is:
  //     [position 0]     0 0 1 1 0 0 0 1 0 1 0 1     [position 11]
  // The string representation will be outputted like this:
  //                     "1 0 1 0 1 0 0 0 1 1 0 0"
  virtual std::string ToString() const = 0;
  virtual void Set(size_t pos, bool value) = 0;
  virtual bool Test(size_t pos) const = 0;
  virtual size_t Size() const = 0;
};

// A bloom filter that gives access to the underlying BitSet. The implementation
// is copied from our Java version of Bloom filter, which in turn copies from
// Guava's BloomFilter.
class BloomFilter {
 public:
  // Constructs by injecting BitSet implementation. The bit_set will be default
  // zero-out.
  explicit BloomFilter(std::unique_ptr<BitSet> bit_set)
      : BloomFilter(std::move(bit_set), {}) {}

  // Constructs by injecting BitSet implementation with bytes of other
  // BloomFilter. The `bit_set` will be filled with the bit_set of other
  // BloomFilter.
  //
  // Note: The capacity size of current bit_set should be the same as the one
  // of other BloomFilter, or there is no impact and fallback to the first
  // constructor.
  BloomFilter(std::unique_ptr<BitSet> bit_set, const ByteArray& bytes);
  BloomFilter(BloomFilter&& other) = default;
  BloomFilter& operator=(BloomFilter&& other) = default;

  explicit operator ByteArray() const;

  void Add(const std::string& s);
  bool PossiblyContains(const std::string& s);

 private:
  std::vector<std::int32_t> GetHashes(const std::string& s);
  int GetMinBytesForBits() const { return (bit_set_->Size() + 7) >> 3; }

  std::unique_ptr<BitSet> bit_set_;
};

// A default bit set implementation.
//
// It is templatized on the size of the byte array and not the size of
// the bit set to ensure the bit set's length is a multiple of 8 (and can
// neatly be returned as a ByteArray).
template <size_t CapacityInBytes>
class BitSetImpl final : public BitSet {
 public:
  std::string ToString() const override { return bits_.to_string(); }
  void Set(size_t pos, bool value) override { bits_.set(pos, value); }
  bool Test(size_t pos) const override { return bits_.test(pos); }
  size_t Size() const override { return bits_.size(); }

 private:
  std::bitset<CapacityInBytes * 8> bits_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_BLE_V2_BLOOM_FILTER_H_
