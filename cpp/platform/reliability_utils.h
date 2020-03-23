#ifndef PLATFORM_RELIABILITY_UTILS_H_
#define PLATFORM_RELIABILITY_UTILS_H_

#include <cstdint>

#include "platform/api/atomic_boolean.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "platform/runnable.h"

namespace location {
namespace nearby {

class ReliabilityUtils {
 public:
  static bool attemptRepeatedly(Ptr<Runnable> runnable,
                                const std::string& runnable_name,
                                Ptr<Runnable> recovery_runnable);
  static bool attemptRepeatedly(Ptr<Runnable> runnable,
                                const std::string& runnable_name,
                                Ptr<Runnable> recovery_runnable,
                                const AtomicBoolean& isCancelled);
  static bool attemptRepeatedly(Ptr<Runnable> runnable,
                                const std::string& runnable_name,
                                std::int64_t recovery_pause_millis);
  static bool attemptRepeatedly(Ptr<Runnable> runnable,
                                const std::string& runnable_name,
                                std::int64_t recovery_pause_millis,
                                const AtomicBoolean& isCancelled);

 private:
  static bool attemptRepeatedly(Ptr<Runnable> runnable,
                                const std::string& runnable_name,
                                int num_attempts,
                                std::int64_t recovery_pause_millis,
                                Ptr<Runnable> recovery_runnable,
                                const AtomicBoolean& isCancelled);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_RELIABILITY_UTILS_H_
