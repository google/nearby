#ifndef PLATFORM_API_INPUT_FILE_H_
#define PLATFORM_API_INPUT_FILE_H_

#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// An InputFile represents a readable file on the system.
class InputFile {
 public:
  virtual ~InputFile() {}

  // The returned ConstPtr will be owned (and destroyed) by the caller.
  // When we have exhausted reading the file and no bytes remain, read will
  // always return an empty ConstPtr for which isNull() is true.
  virtual ExceptionOr<ConstPtr<ByteArray>> read(
      std::int64_t size) = 0;  // throws Exception::IO when the file cannot be
                               // opened or read.
  virtual std::string getFilePath() const = 0;
  virtual std::int64_t getTotalSize() const = 0;
  virtual void close() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_INPUT_FILE_H_
