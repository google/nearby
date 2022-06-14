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

#include "internal/platform/implementation/shared/file.h"

#include <sys/stat.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "internal/platform/exception.h"

namespace location {
namespace nearby {
namespace shared {

// InputFile
std::unique_ptr<IOFile> IOFile::CreateInputFile(
    const absl::string_view file_path, size_t size) {
  return absl::WrapUnique(new IOFile(file_path, size));
}

IOFile::IOFile(const absl::string_view file_path, size_t size)
    : file_(std::string(file_path.data(), file_path.size()),
            std::ios::binary | std::ios::in),
      path_(file_path),
      total_size_(size) {}

std::unique_ptr<IOFile> IOFile::CreateOutputFile(const absl::string_view path) {
  return std::unique_ptr<IOFile>(new IOFile(path));
}

std::fstream& IOFile::CreateOutputFileWithRename(absl::string_view path) {
  std::string full_path(path);
  std::replace(full_path.begin(), full_path.end(), '\\', '/');
  std::string folder(full_path.substr(0, full_path.find_last_of('/')));
  std::string file_name(full_path.substr(full_path.find_last_of('/')));
  std::string increment_string;

  int count = 0;

  auto first = file_name.find_first_of('.', 0);

  if (first == std::string::npos) {
    first = file_name.size();
  }

  auto file_name1 = file_name.substr(0, first);
  auto file_name2 = file_name.substr(first);

  std::string target = absl::StrCat(folder, file_name1, file_name2);
  file_.open(target, std::ios::binary | std::ios::in);

  while (!(file_.rdstate() & std::ifstream::failbit)) {
    file_.close();
    increment_string = absl::StrCat(" (", ++count, ")");
    target = absl::StrCat(folder, file_name1, increment_string, file_name2);
    file_.clear();
    file_.open(target, std::ios::binary | std::ios::in);
  }

  file_.close();

  path_.append(target);

  file_.clear();
  file_.open(path_, std::ios::binary | std::ios::out);

  return file_;
}

IOFile::IOFile(const absl::string_view file_path) : file_(), total_size_(0) {
  CreateOutputFileWithRename(file_path);
}

ExceptionOr<ByteArray> IOFile::Read(std::int64_t size) {
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

Exception IOFile::Close() {
  if (file_.is_open()) {
    file_.close();
  }
  return {Exception::kSuccess};
}

Exception IOFile::Write(const ByteArray& data) {
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

Exception IOFile::Flush() {
  file_.flush();
  return {file_.good() ? Exception::kSuccess : Exception::kIo};
}

}  // namespace shared
}  // namespace nearby
}  // namespace location
