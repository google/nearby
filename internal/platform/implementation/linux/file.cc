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

#include "internal/platform/implementation/linux/file.h"

#include <algorithm>
#include <chrono>
#include <codecvt>
#include <cstddef>
#include <filesystem>
#include <ios>
#include <locale>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/utils.h"

namespace nearby {
namespace linux {

// InputFile
std::unique_ptr<IOFile> IOFile::CreateInputFile(
    const absl::string_view file_path, size_t size) {
  return absl::WrapUnique(new IOFile(file_path, size));
}

IOFile::IOFile(const absl::string_view file_path, size_t size)
    : path_(file_path) {
  file_.open(std::filesystem::path(path_),
             std::ios::binary | std::ios::in | std::ios::ate);

  total_size_ = file_.tellg();
  file_.seekg(0);
}

std::unique_ptr<IOFile> IOFile::CreateOutputFile(const absl::string_view path) {
  return std::unique_ptr<IOFile>(new IOFile(path));
}

IOFile::IOFile(const absl::string_view file_path)
    : file_(), path_(file_path), total_size_(0) {
  file_.open(std::filesystem::path(path_),
             std::ios::binary | std::ios::out);
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

absl::Time IOFile::GetLastModifiedTime() const {
  std::filesystem::path file_path(path_);
  if (!std::filesystem::exists(file_path)) {
    return absl::InfinitePast();
  }
  auto ftime = std::filesystem::last_write_time(file_path);
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() +
      std::chrono::system_clock::now());
  return absl::FromChrono(sctp);
}

void IOFile::SetLastModifiedTime(absl::Time last_modified_time) {
  std::filesystem::path file_path(path_);
  if (!std::filesystem::exists(file_path)) {
    return;
  }
  auto chrono_time = absl::ToChronoTime(last_modified_time);
  auto ftime = std::filesystem::file_time_type::clock::now() +
               (chrono_time - std::chrono::system_clock::now());
  std::filesystem::last_write_time(file_path, ftime);
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

}  // namespace linux
}  // namespace nearby
