#ifndef PLATFORM_V2_API_SYSTEM_CLOCK_H_
#define PLATFORM_V2_API_SYSTEM_CLOCK_H_

#include "platform_v2/base/exception.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {

class SystemClock final {
 public:
  // Initialize global system state.
  static void Init();
  // Returns current absolute time. It is guaranteed to be monotonic.
  static absl::Time ElapsedRealtime();
  // Pauses current thread for the specified duration.
  static Exception Sleep(absl::Duration duration);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_SYSTEM_CLOCK_H_
