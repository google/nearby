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

#include "internal/platform/implementation/windows/input_file.h"

#include <fileapi.h>

#include <cstdint>
#include <limits>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/logging.h"

namespace nearby::windows {
namespace {

const absl::Time kFiletimeEpoch =
    absl::FromCivil(absl::CivilYear(1601), absl::UTCTimeZone());

constexpr absl::Duration kFiletimeUnit = absl::Nanoseconds(100);
const int64_t kFiletimeUnitsPerSecond = absl::Seconds(1) / kFiletimeUnit;

absl::Duration DurationFromFiletime(const FILETIME& file_time) {
  LONGLONG quad_date_time =
      (static_cast<LONGLONG>(file_time.dwHighDateTime) << 32) |
      static_cast<LONGLONG>(file_time.dwLowDateTime);

  const absl::Duration subsecond =
      kFiletimeUnit * (quad_date_time % kFiletimeUnitsPerSecond);
  const absl::Duration seconds =
      absl::Seconds(quad_date_time / kFiletimeUnitsPerSecond);
  return seconds + subsecond;
}

absl::Time TimeFromFiletime(const FILETIME& file_time) {
  return kFiletimeEpoch + DurationFromFiletime(file_time);
}

}  // namespace

InputFile::InputFile(absl::string_view file_path) : path_(file_path) {
  // Always open input file path as wide string on Windows platform.
  std::wstring wide_path = string_utils::StringToWideString(path_);
  file_ = ::CreateFileW(wide_path.data(), GENERIC_READ, FILE_SHARE_READ,
                        /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                        /*hTemplateFile=*/nullptr);
  if (file_ == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to open input file: " << path_
               << " with error: " << ::GetLastError();
    return;
  }

  LARGE_INTEGER file_size;
  if (::GetFileSizeEx(file_, &file_size) == 0) {
    LOG(ERROR) << "Failed to get file size: " << path_
               << " with error: " << ::GetLastError();
    ::CloseHandle(file_);
    file_ = INVALID_HANDLE_VALUE;
    return;
  }
  total_size_ = file_size.QuadPart;
}

ExceptionOr<ByteArray> InputFile::Read(std::int64_t size) {
  if (file_ == INVALID_HANDLE_VALUE) {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }
  // ReadFile API only supports int32_t size.
  if (size < 0 || size > std::numeric_limits<std::uint32_t>::max()) {
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

Exception InputFile::Close() {
  if (file_ != INVALID_HANDLE_VALUE) {
    ::CloseHandle(file_);
    file_ = INVALID_HANDLE_VALUE;
  }
  return {Exception::kSuccess};
}

absl::Time InputFile::GetLastModifiedTime() const {
  if (file_ == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to get file modified time for: " << path_;
    return absl::Now();
  }
  FILETIME last_write_time;
  if (::GetFileTime(file_, /*lpCreationTime=*/nullptr,
                    /*lpLastAccessTime=*/nullptr, &last_write_time) == 0) {
    LOG(ERROR) << "Failed to get file last write time: " << path_
               << " with error: " << ::GetLastError();
    return absl::Now();
  }
  return TimeFromFiletime(last_write_time);
}

}  // namespace nearby::windows
