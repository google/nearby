#ifndef PLATFORM_IMPL_LINUX_STREAM_H_
#define PLATFORM_IMPL_LINUX_STREAM_H_

#include <optional>

#include <sdbus-c++/Types.h>

#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace linux {
class InputStream : public nearby::InputStream {
public:
  InputStream(sdbus::UnixFd &fd) : fd_(fd){};

  ExceptionOr<ByteArray> Read(std::int64_t size) override;

  Exception Close() override;

private:
  std::optional<sdbus::UnixFd> fd_;
};

class OutputStream : public nearby::OutputStream {
public:
  OutputStream(sdbus::UnixFd &fd) : fd_(fd){};

  Exception Write(const ByteArray &data) override;
  Exception Flush() override;
  Exception Close() override;

private:
  std::optional<sdbus::UnixFd> fd_;
};

} // namespace linux
} // namespace nearby

#endif
