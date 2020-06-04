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

#ifndef CORE_V2_INTERNAL_MEDIUMS_BLOOM_FILTER_H_
#define CORE_V2_INTERNAL_MEDIUMS_BLOOM_FILTER_H_

#include <bitset>
#include <vector>

#include "platform_v2/base/byte_array.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

/**
 * A bloom filter that gives access to the underlying BitSet. The implementation
 * is copied from our Java version of Bloom filter, which in turn copies from
 * Guava's BloomFilter.
 *
 * BloomFilter is templatized on the size of the byte array and not the size of
 * the bit set to ensure the bit set's length is a multiple of 8 (and can
 * neatly be returned as a ByteArray).
 */
class BloomFilterBase {
 public:
  explicit operator ByteArray() const;

  void Add(const std::string& s);
  bool PossiblyContains(const std::string& s);

 protected:
  class BitSet {
   public:
    virtual ~BitSet() = default;
    virtual std::string ToString() const = 0;
    virtual void Set(size_t pos, bool value) = 0;
    virtual bool Test(size_t pos) const = 0;
    virtual size_t Size() const = 0;
  };

  BloomFilterBase(const ByteArray& bytes, BitSet* bit_set);
  virtual ~BloomFilterBase() = default;

  constexpr static int kHasherNumberOfRepetitions = 5;
  std::vector<std::int32_t> GetHashes(const std::string& s);

 private:
  int GetMinBytesForBits() const { return (bits_->Size() + 7) >> 3; }

  BitSet* bits_;
};

template <size_t CapacityInBytes>
class BloomFilter final : public BloomFilterBase {
 public:
  BloomFilter() : BloomFilterBase(ByteArray{}, &bits_) {}
  explicit BloomFilter(const ByteArray& bytes)
      : BloomFilterBase(bytes, &bits_) {}
  BloomFilter(const BloomFilter&) = default;
  BloomFilter& operator=(const BloomFilter&) = default;
  BloomFilter(BloomFilter&& other) : BloomFilterBase(ByteArray{}, &bits_) {
    *this = std::move(other);
  }
  BloomFilter& operator=(BloomFilter&& other) {
    std::swap((*this).bits_, other.bits_);
    return *this;
  }
  ~BloomFilter() override = default;

 private:
  class BitSetImpl final : public BitSet {
   public:
    std::string ToString() const override { return bits_.to_string(); }
    void Set(size_t pos, bool value) override { bits_.set(pos, value); }
    bool Test(size_t pos) const override { return bits_.test(pos); }
    size_t Size() const override { return bits_.size(); }

   private:
    std::bitset<CapacityInBytes * 8> bits_;
  } bits_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_BLOOM_FILTER_H_
