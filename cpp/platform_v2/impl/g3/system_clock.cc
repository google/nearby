#include "platform_v2/api/system_clock.h"

#include "platform_v2/base/exception.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {

absl::Time SystemClock::ElapsedRealtime() { return absl::Now(); }
Exception SystemClock::Sleep(absl::Duration duration) {
  absl::SleepFor(duration);
  return {Exception::kSuccess};
}

}  // namespace nearby
}  // namespace location
