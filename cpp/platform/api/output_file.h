#ifndef PLATFORM_API_OUTPUT_FILE_H_
#define PLATFORM_API_OUTPUT_FILE_H_

#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// An OutputFile represents a writable file on the system.
class OutputFile {
 public:
  virtual ~OutputFile() {}

  // Takes ownership of the passed-in ConstPtr, and ensures that it is destroyed
  // even upon error (i.e. when the return value is not Exception::NONE).
  virtual Exception::Value write(
      ConstPtr<ByteArray> data) = 0;  // throws Exception::IO
  virtual void close() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_OUTPUT_FILE_H_
