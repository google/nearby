#ifndef PLATFORM_API2_INPUT_STREAM_H_
#define PLATFORM_API2_INPUT_STREAM_H_

#include <cstdint>

#include "platform/byte_array.h"
#include "platform/exception.h"

namespace location {
namespace nearby {

// An InputStream represents an input stream of bytes.
//
// https://docs.oracle.com/javase/8/docs/api/java/io/InputStream.html
class InputStream {
 public:
  virtual ~InputStream() {}

  virtual ExceptionOr<ByteArray> Read(
      size_t size) = 0;           // throws Exception::kIo
  virtual Exception Close() = 0;  // throws Exception::kIo
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_INPUT_STREAM_H_
