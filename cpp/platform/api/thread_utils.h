#ifndef PLATFORM_API_THREAD_UTILS_H_
#define PLATFORM_API_THREAD_UTILS_H_

#include <cstdint>

#include "platform/exception.h"

namespace location {
namespace nearby {

class ThreadUtils {
 public:
  virtual ~ThreadUtils() {}

  // https://docs.oracle.com/javase/7/docs/api/java/lang/Thread.html#sleep(long)
  virtual Exception::Value sleep(
      std::int64_t millis) = 0;  // throws Exception::INTERRUPTED
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_THREAD_UTILS_H_
