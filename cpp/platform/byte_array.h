#ifndef PLATFORM_BYTE_ARRAY_H_
#define PLATFORM_BYTE_ARRAY_H_

#include "platform/port/string.h"

namespace location {
namespace nearby {

class ByteArray {
 public:
  // Create an empty ByteArray
  ByteArray() {}

  // Create ByteArray from string.
  explicit ByteArray(const std::string& source) {
    data_ = source;
  }

  // Create default-initialized ByteArray of a given size.
  explicit ByteArray(size_t size) {
    setData(size);
  }

  // Create value-initialized ByteArray of a given size.
  ByteArray(const char* data, size_t size) {
    setData(data, size);
  }

  // Assign a new value to this ByteArray, as a copy of data, with a given size.
  void setData(const char* data, size_t size) {
    data_.assign(data, size);
  }

  // Assign a new value of a given size to this ByteArray
  // (as a repeated char value).
  void setData(size_t size, char value = 0) {
    data_.assign(size, value);
  }

  char* getData() { return data_.data(); }
  const char* getData() const { return data_.data(); }
  size_t size() const { return data_.size(); }

  // Operator overloads when comparing ConstPtr<ByteArray>.
  bool operator==(const ByteArray& rhs) const {
    return this->data_ == rhs.data_;
  }
  bool operator!=(const ByteArray& rhs) const { return !(*this == rhs); }
  bool operator<(const ByteArray& rhs) const {
    return this->data_ < rhs.data_;
  }
  // TODO(b/149869249) : rename according to go/c-style
  std::string asString() const { return data_; }

 private:
  std::string data_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BYTE_ARRAY_H_
