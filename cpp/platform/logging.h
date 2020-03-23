#ifndef PLATFORM_LOGGING_H_
#define PLATFORM_LOGGING_H_

#include "absl/base/internal/raw_logging.h"

namespace location {
namespace nearby {

// This uses an explicit printf-format and arguments list, and supports the
// following severities:
//
//    - INFO
//    - WARNING
//    - ERROR
//    - FATAL
//
// To make it easy to filer while debugging, it prepends "[NEARBY] " to all its
// logged messages.
//
// Sample usage:
//
//      NEARBY_LOG(INFO, "%d is an int and %s is a std::string", i, s.c_str());
#define NEARBY_LOG(severity, ...) \
  ABSL_RAW_LOG(severity, "[NEARBY] " __VA_ARGS__)

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_LOGGING_H_
