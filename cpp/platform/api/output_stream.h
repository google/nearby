#ifndef PLATFORM_API_OUTPUT_STREAM_H_
#define PLATFORM_API_OUTPUT_STREAM_H_

#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// An OutputStream represents an output stream of bytes.
//
// https://docs.oracle.com/javase/8/docs/api/java/io/OutputStream.html
class OutputStream {
 public:
  virtual ~OutputStream() {}

  // Takes ownership of the passed-in ConstPtr, and ensures that it is destroyed
  // even upon error (i.e. when the return value is not Exception::NONE).
  virtual Exception::Value write(
      ConstPtr<ByteArray> data) = 0;     // throws Exception::IO
  virtual Exception::Value flush() = 0;  // throws Exception::IO
  virtual Exception::Value close() = 0;  // throws Exception::IO
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_OUTPUT_STREAM_H_
