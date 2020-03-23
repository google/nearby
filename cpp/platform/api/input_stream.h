#ifndef PLATFORM_API_INPUT_STREAM_H_
#define PLATFORM_API_INPUT_STREAM_H_

#include <cstdint>

#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// An InputStream represents an input stream of bytes.
//
// https://docs.oracle.com/javase/8/docs/api/java/io/InputStream.html
class InputStream {
 public:
  virtual ~InputStream() {}

  // The returned ConstPtr will be owned (and destroyed) by the caller.
  virtual ExceptionOr<ConstPtr<ByteArray> > read() = 0;  // throws Exception::IO
  // The returned ConstPtr will be owned (and destroyed) by the caller.
  virtual ExceptionOr<ConstPtr<ByteArray> > read(
      std::int64_t size) = 0;            // throws Exception::IO
  virtual Exception::Value close() = 0;  // throws Exception::IO
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_INPUT_STREAM_H_
