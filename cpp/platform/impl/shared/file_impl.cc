#include "platform/impl/shared/file_impl.h"

#include <cstddef>
#include <memory>

namespace location {
namespace nearby {

// InputFile

InputFileImpl::InputFileImpl(const std::string& path, std::int64_t size)
    : file_(path), path_(path), total_size_(size) {}

ExceptionOr<ConstPtr<ByteArray>> InputFileImpl::read(int64_t size) {
  if (!file_.is_open()) {
    return ExceptionOr<ConstPtr<ByteArray>>(Exception::IO);
  }

  if (file_.peek() == EOF) {
    return ExceptionOr<ConstPtr<ByteArray>>(ConstPtr<ByteArray>());
  }

  if (!file_.good()) {
    return ExceptionOr<ConstPtr<ByteArray>>(Exception::IO);
  }

  std::unique_ptr<char[]> read_bytes {new char [size]};
  file_.read(read_bytes.get(), static_cast<ptrdiff_t>(size));
  auto num_bytes_read = file_.gcount();
  if (num_bytes_read == 0) {
    return ExceptionOr<ConstPtr<ByteArray>>(Exception::IO);
  }

  return ExceptionOr<ConstPtr<ByteArray>>(
      MakeConstPtr(new ByteArray(read_bytes.get(), num_bytes_read)));
}

std::string InputFileImpl::getFilePath() const { return path_; }

std::int64_t InputFileImpl::getTotalSize() const { return total_size_; }

void InputFileImpl::close() {
  if (file_.is_open()) {
    file_.close();
  }
}

// OutputFile

OutputFileImpl::OutputFileImpl(const std::string& path) : file_(path) {}

Exception::Value OutputFileImpl::write(ConstPtr<ByteArray> data) {
  ScopedPtr<ConstPtr<ByteArray>> scoped_data(data);

  if (!file_.is_open()) {
    return Exception::IO;
  }

  if (!file_.good()) {
    return Exception::IO;
  }

  file_.write(data->getData(), data->size());
  file_.flush();
  return file_.good() ? Exception::NONE : Exception::IO;
}

void OutputFileImpl::close() {
  if (file_.is_open()) {
    file_.close();
  }
}

}  // namespace nearby
}  // namespace location
