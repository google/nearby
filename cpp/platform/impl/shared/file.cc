#include "platform/impl/shared/file.h"

#include <cstddef>
#include <memory>

#include "platform/base/exception.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace shared {

// InputFile

InputFile::InputFile(const std::string& path, std::int64_t size)
    : file_(path), path_(path), total_size_(size) {}

ExceptionOr<ByteArray> InputFile::Read(std::int64_t size) {
  if (!file_.is_open()) {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }

  if (file_.peek() == EOF) {
    return ExceptionOr<ByteArray>{ByteArray{}};
  }

  if (!file_.good()) {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }

  ByteArray bytes(size);
  std::unique_ptr<char[]> read_bytes{new char[size]};
  file_.read(read_bytes.get(), static_cast<ptrdiff_t>(size));
  auto num_bytes_read = file_.gcount();
  if (num_bytes_read == 0) {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }

  return ExceptionOr<ByteArray>(ByteArray(read_bytes.get(), num_bytes_read));
}

Exception InputFile::Close() {
  if (file_.is_open()) {
    file_.close();
  }
  return {Exception::kSuccess};
}

// OutputFile

OutputFile::OutputFile(absl::string_view path) : file_(std::string(path)) {}

Exception OutputFile::Write(const ByteArray& data) {
  if (!file_.is_open()) {
    return {Exception::kIo};
  }

  if (!file_.good()) {
    return {Exception::kIo};
  }

  file_.write(data.data(), data.size());
  file_.flush();
  return {file_.good() ? Exception::kSuccess : Exception::kIo};
}

Exception OutputFile::Flush() {
  file_.flush();
  return {file_.good() ? Exception::kSuccess : Exception::kIo};
}

Exception OutputFile::Close() {
  if (file_.is_open()) {
    file_.close();
  }
  return {Exception::kSuccess};
}

}  // namespace shared
}  // namespace nearby
}  // namespace location
