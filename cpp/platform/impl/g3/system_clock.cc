#include "platform/api/system_clock.h"

#include "platform/base/exception.h"
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
