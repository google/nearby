// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "platform/impl/shared/file.h"

#include <cstddef>
#include <memory>

#include "absl/strings/string_view.h"
#include "platform/base/exception.h"

namespace location {
namespace nearby {
namespace shared {

// InputFile
InputFile::InputFile(const char* file_path) : file_path_(file_path) {
  // Open the file with the current location at eof (std::ios::ate)
  // std::ios::binary - specifies binary access mode
  // std::ios::in - allows input (read operations) from a stream
  // std::ios::ate - sets the stream's position indicator to the
  //                 end of the stream on opening.
  file_.open(std::string(file_path),
             std::ios::binary | std::ios::in | std::ios::ate);

  // Read the current position in the file and use that for total size.
  total_size_ = file_.tellg();

  // Reset to the beginning of the file.
  file_.seekg(0);
}

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

OutputFile::OutputFile(const char* file_path) {
  // std::ios::binary - specifies binary access mode
  // std::ios::out - allows output (read operations) from
  //                 a stream
  // std::ios::trunc - when the file is opened, the old
  //                   contents are immediately removed.
  file_ = std::ofstream(file_path,
                        std::ios::binary | std::ios::out | std::ios::trunc);
}

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
