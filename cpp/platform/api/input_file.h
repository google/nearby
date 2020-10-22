#ifndef PLATFORM_API_INPUT_FILE_H_
#define PLATFORM_API_INPUT_FILE_H_

#include <cstdint>

#include "platform/base/byte_array.h"
#include "platform/base/exception.h"
#include "platform/base/input_stream.h"

namespace location {
namespace nearby {
namespace api {

// An InputFile represents a readable file on the system.
class InputFile : public InputStream {
 public:
  ~InputFile() override = default;
  virtual std::string GetFilePath() const = 0;
  virtual std::int64_t GetTotalSize() const = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_INPUT_FILE_H_
