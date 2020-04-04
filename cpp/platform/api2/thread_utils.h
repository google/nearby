#ifndef PLATFORM_API2_THREAD_UTILS_H_
#define PLATFORM_API2_THREAD_UTILS_H_

#include <cstdint>

#include "platform/exception.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {

class ThreadUtils final {
 public:
  // https://docs.oracle.com/javase/7/docs/api/java/lang/Thread.html#sleep(long)
  // throws Exception::kInterrupted
  static Exception Sleep(absl::Duration timeout);
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_THREAD_UTILS_H_
