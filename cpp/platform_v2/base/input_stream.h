#ifndef PLATFORM_V2_BASE_INPUT_STREAM_H_
#define PLATFORM_V2_BASE_INPUT_STREAM_H_

#include <cstdint>

#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/exception.h"

namespace location {
namespace nearby {

// An InputStream represents an input stream of bytes.
//
// https://docs.oracle.com/javase/8/docs/api/java/io/InputStream.html
class InputStream {
 public:
  virtual ~InputStream() = default;

  // throws Exception::kIo
  virtual ExceptionOr<ByteArray> Read(std::int64_t size) = 0;
  // throws Exception::kIo
  virtual Exception Close() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_BASE_INPUT_STREAM_H_
