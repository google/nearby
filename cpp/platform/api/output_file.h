#ifndef PLATFORM_API_OUTPUT_FILE_H_
#define PLATFORM_API_OUTPUT_FILE_H_

#include "platform/base/byte_array.h"
#include "platform/base/exception.h"
#include "platform/base/output_stream.h"

namespace location {
namespace nearby {
namespace api {

// An OutputFile represents a writable file on the system.
class OutputFile : public OutputStream {
 public:
  ~OutputFile() override = default;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_OUTPUT_FILE_H_
