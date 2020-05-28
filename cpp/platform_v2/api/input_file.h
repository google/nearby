#ifndef PLATFORM_V2_API_INPUT_FILE_H_
#define PLATFORM_V2_API_INPUT_FILE_H_

#include <cstdint>

#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/exception.h"
#include "platform_v2/base/input_stream.h"

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

#endif  // PLATFORM_V2_API_INPUT_FILE_H_
