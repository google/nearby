#ifndef CORE_INTERNAL_MEDIUMS_BLOOM_FILTER_H_
#define CORE_INTERNAL_MEDIUMS_BLOOM_FILTER_H_

#include <bitset>
#include <cstdint>
#include <vector>

#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

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
template <size_t CapacityInBytes>
class BloomFilter {
 public:
  BloomFilter();
  explicit BloomFilter(ConstPtr<ByteArray> bytes);
  ~BloomFilter();

  ConstPtr<ByteArray> asBytes();

  void add(const std::string& s);

  bool possiblyContains(const std::string& s);

 private:
  static const std::int32_t kHasherNumberOfRepetitions;

  std::vector<std::int32_t> getHashes(const std::string& s);

  std::bitset<CapacityInBytes * 8> bits_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/bloom_filter.cc"

#endif  // CORE_INTERNAL_MEDIUMS_BLOOM_FILTER_H_
