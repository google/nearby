#ifndef PLATFORM_IMPL_G3_SYSTEM_CLOCK_IMPL_H_
#define PLATFORM_IMPL_G3_SYSTEM_CLOCK_IMPL_H_

#include <cstdint>

#include "platform/api/system_clock.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

class SystemClockImpl : public SystemClock {
 public:
  std::int64_t elapsedRealtime() override {
    return absl::ToUnixMillis(absl::Now());
  }
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_G3_SYSTEM_CLOCK_IMPL_H_
