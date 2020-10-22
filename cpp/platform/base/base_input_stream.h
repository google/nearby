#ifndef PLATFORM_BASE_BASE_INPUT_STREAM_H_
#define PLATFORM_BASE_BASE_INPUT_STREAM_H_

#include "platform/base/byte_array.h"
#include "platform/base/exception.h"
#include "platform/base/input_stream.h"

namespace location {
namespace nearby {

// A base {@link InputStream } for reading the contents of a byte array.
class BaseInputStream : public InputStream {
 public:
  explicit BaseInputStream(ByteArray &buffer) : buffer_{buffer} {}
  BaseInputStream(const BaseInputStream &) = delete;
  BaseInputStream &operator=(const BaseInputStream &) = delete;
  ~BaseInputStream() override { Close(); }

  ExceptionOr<ByteArray> Read(std::int64_t size) override;

  Exception Close() override {
    // Do nothing.
    return {Exception::kSuccess};
  }

  std::uint8_t ReadUint8();
  std::uint16_t ReadUint16();
  std::uint32_t ReadUint32();
  std::uint64_t ReadUint64();
  ByteArray ReadBytes(int size);
  bool IsAvailable(int size) const {
    return buffer_.size() - position_ >= size;
  }

 private:
  ByteArray &buffer_;
  int position_{0};
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_BASE_INPUT_STREAM_H_
