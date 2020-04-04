#ifndef PLATFORM_API2_SYSTEM_CLOCK_H_
#define PLATFORM_API2_SYSTEM_CLOCK_H_

#include <cstdint>

#include "absl/time/time.h"

namespace location {
namespace nearby {

class SystemClock final {
 public:
  // Returns the time (in milliseconds) since the system was booted, and
  // includes deep sleep. This clock should be guaranteed to be monotonic, and
  // should continue to tick even when the CPU is in power saving modes.
  static absl::Time ElapsedRealtime();
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_SYSTEM_CLOCK_H_
