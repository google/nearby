#ifndef PLATFORM_V2_API_OUTPUT_FILE_H_
#define PLATFORM_V2_API_OUTPUT_FILE_H_

#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/exception.h"
#include "platform_v2/base/output_stream.h"

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

#endif  // PLATFORM_V2_API_OUTPUT_FILE_H_
