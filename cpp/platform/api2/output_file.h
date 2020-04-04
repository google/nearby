#ifndef PLATFORM_API2_OUTPUT_FILE_H_
#define PLATFORM_API2_OUTPUT_FILE_H_

#include "platform/api2/output_stream.h"
#include "platform/byte_array.h"
#include "platform/exception.h"

namespace location {
namespace nearby {

// An OutputFile represents a writable file on the system.
class OutputFile : public OutputStream {
 public:
  ~OutputFile() override = default;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_OUTPUT_FILE_H_
