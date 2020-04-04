#ifndef PLATFORM_API2_INPUT_FILE_H_
#define PLATFORM_API2_INPUT_FILE_H_

#include <cstdint>

#include "platform/api2/input_stream.h"
#include "platform/byte_array.h"
#include "platform/exception.h"

namespace location {
namespace nearby {

// An InputFile represents a readable file on the system.
class InputFile : public InputStream {
 public:
  ~InputFile() override = default;
  virtual std::string GetFilePath() const = 0;
  virtual size_t GetTotalSize() const = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_INPUT_FILE_H_
