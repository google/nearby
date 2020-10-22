#ifndef PLATFORM_BASE_PRNG_H_
#define PLATFORM_BASE_PRNG_H_

#include <cstdint>

namespace location {
namespace nearby {

// A (non-cryptographic) pseudo-random number generator.
class Prng {
 public:
  Prng();
  ~Prng();

  std::int32_t NextInt32();
  std::uint32_t NextUint32();
  std::int64_t NextInt64();
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_PRNG_H_
