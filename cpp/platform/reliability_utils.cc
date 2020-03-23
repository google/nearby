#include "platform/reliability_utils.h"

namespace location {
namespace nearby {

bool ReliabilityUtils::attemptRepeatedly(Ptr<Runnable> runnable,
                                         const std::string &runnable_name,
                                         Ptr<Runnable> recovery_runnable) {
  return false;
}

bool ReliabilityUtils::attemptRepeatedly(Ptr<Runnable> runnable,
                                         const std::string &runnable_name,
                                         Ptr<Runnable> recovery_runnable,
                                         const AtomicBoolean &isCancelled) {
  return false;
}

bool ReliabilityUtils::attemptRepeatedly(Ptr<Runnable> runnable,
                                         const std::string &runnable_name,
                                         std::int64_t recovery_pause_millis) {
  return false;
}

bool ReliabilityUtils::attemptRepeatedly(Ptr<Runnable> runnable,
                                         const std::string &runnable_name,
                                         std::int64_t recovery_pause_millis,
                                         const AtomicBoolean &isCancelled) {
  return false;
}

bool ReliabilityUtils::attemptRepeatedly(Ptr<Runnable> runnable,
                                         const std::string &runnable_name,
                                         int num_attempts,
                                         std::int64_t recovery_pause_millis,
                                         Ptr<Runnable> recovery_runnable,
                                         const AtomicBoolean &isCancelled) {
  return false;
}

}  // namespace nearby
}  // namespace location
