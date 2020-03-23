#ifndef PLATFORM_API_SYSTEM_CLOCK_H_
#define PLATFORM_API_SYSTEM_CLOCK_H_

#include <cstdint>

namespace location {
namespace nearby {

class SystemClock {
 public:
  virtual ~SystemClock() {}

  // Returns the time (in milliseconds) since the system was booted, and
  // includes deep sleep. This clock should be guaranteed to be monotonic, and
  // should continue to tick even when the CPU is in power saving modes.
  virtual std::int64_t elapsedRealtime() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SYSTEM_CLOCK_H_
