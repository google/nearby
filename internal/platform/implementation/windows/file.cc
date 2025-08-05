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

#include "internal/platform/implementation/windows/file.h"

#include <fileapi.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

// InputFile
std::unique_ptr<IOFile> IOFile::CreateInputFile(absl::string_view file_path,
                                                size_t size) {
  return absl::WrapUnique(new IOFile(file_path, size));
}

IOFile::IOFile(absl::string_view file_path, size_t size) : path_(file_path) {
  // Always open input file path as wide string on Windows platform.
  std::wstring wide_path = string_utils::StringToWideString(path_);
  file_ = ::CreateFileW(wide_path.data(), GENERIC_READ, FILE_SHARE_READ,
                        /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                        /*hTemplateFile=*/nullptr);
  if (file_ == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to open input file: " << file_path
               << " with error: " << ::GetLastError();
    return;
  }

  LARGE_INTEGER file_size;
  if (::GetFileSizeEx(file_, &file_size) == 0) {
    LOG(ERROR) << "Failed to get file size: " << file_path
               << " with error: " << ::GetLastError();
    return;
  }
  total_size_ = file_size.QuadPart;
}

std::unique_ptr<IOFile> IOFile::CreateOutputFile(absl::string_view path) {
  return std::unique_ptr<IOFile>(new IOFile(path));
}

IOFile::IOFile(absl::string_view file_path) : path_(file_path), total_size_(0) {
  // Always open input file path as wide string on Windows platform.
  std::wstring wide_path = string_utils::StringToWideString(path_);
  file_ = ::CreateFileW(wide_path.data(), GENERIC_WRITE, /*dwShareMode=*/0,
                        /*lpSecurityAttributes=*/nullptr, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL,
                        /*hTemplateFile=*/nullptr);
  if (file_ == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to open output file: " << file_path
               << " with error: " << ::GetLastError();
    return;
  }
}

ExceptionOr<ByteArray> IOFile::Read(std::int64_t size) {
  if (file_ == INVALID_HANDLE_VALUE) {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }
  // ReadFile API only supports int32_t size.
  if (size > std::numeric_limits<std::uint32_t>::max()) {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }
  if (buffer_.size() < size) {
    buffer_.resize(size);
  }
  DWORD bytes_read = 0;
  if (::ReadFile(file_, buffer_.data(), size, &bytes_read,
                 /*lpOverlapped=*/nullptr) == 0) {
    LOG(ERROR) << "Failed to read file: " << path_
               << " with error: " << ::GetLastError();
    return ExceptionOr<ByteArray>{Exception::kIo};
  }
  if (bytes_read == 0) {
    return ExceptionOr<ByteArray>{ByteArray{}};
  }
  return ExceptionOr<ByteArray>(ByteArray(buffer_.data(), bytes_read));
}

Exception IOFile::Close() {
  if (file_ != INVALID_HANDLE_VALUE) {
    ::CloseHandle(file_);
    file_ = INVALID_HANDLE_VALUE;
  }
  return {Exception::kSuccess};
}

Exception IOFile::Write(const ByteArray& data) {
  if (file_ == INVALID_HANDLE_VALUE) {
    return {Exception::kIo};
  }
  // WriteFile API only supports int32_t size.
  if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
    return {Exception::kIo};
  }
  DWORD bytes_written = 0;
  if (::WriteFile(file_, data.data(), data.size(), &bytes_written,
                  /*lpOverlapped=*/nullptr) == 0 ||
      bytes_written != data.size()) {
    LOG(ERROR) << "Failed to write file: " << path_
               << " with error: " << ::GetLastError();
    return {Exception::kIo};
  }
  return {Exception::kSuccess};
}

}  // namespace nearby::windows
