#ifndef PLATFORM_PRNG_H_
#define PLATFORM_PRNG_H_

#include <cstdint>

namespace location {
namespace nearby {

// A (non-cryptographic) pseudo-random number generator.
class Prng {
 public:
  Prng();
  ~Prng();

  std::int32_t nextInt32();
  std::uint32_t nextUInt32();
  std::int64_t nextInt64();
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PRNG_H_
