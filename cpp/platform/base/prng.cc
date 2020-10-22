#include "platform/base/prng.h"

#include <limits>

#include "absl/time/clock.h"

namespace location {
namespace nearby {

#define UNSIGNED_INT_BITMASK (std::numeric_limits<unsigned int>::max())

Prng::Prng() {
  // absl::GetCurrentTimeNanos() returns 64 bits, but srand() wants an unsigned
  // int, so we may have to lose some of those 64 bits.
  //
  // The lower bits of the current-time-in-nanos are likely to have more entropy
  // than the upper bits, so choose the former.
  srand(static_cast<unsigned int>(absl::GetCurrentTimeNanos() &
                                  UNSIGNED_INT_BITMASK));
}

Prng::~Prng() {
  // Nothing to do.
}

#define RANDOM_BYTE (rand() & 0x0FF)  // NOLINT

std::int32_t Prng::NextInt32() {
  return (static_cast<std::int32_t>(RANDOM_BYTE) << 24) |
         (static_cast<std::int32_t>(RANDOM_BYTE) << 16) |
         (static_cast<std::int32_t>(RANDOM_BYTE) << 8) |
         (static_cast<std::int32_t>(RANDOM_BYTE));
}

std::uint32_t Prng::NextUint32() {
  return static_cast<std::uint32_t>(NextInt32());
}

std::int64_t Prng::NextInt64() {
  return (static_cast<std::int64_t>(NextInt32()) << 32) |
         (static_cast<std::int64_t>(NextUint32()));
}

}  // namespace nearby
}  // namespace location
