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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

namespace {
static const absl::Time kFiletimeEpoch =
    absl::FromCivil(absl::CivilYear(1601), absl::UTCTimeZone());

constexpr absl::Duration kFiletimeUnit = absl::Nanoseconds(100);
const int64_t kFiletimeUnitsPerSecond = absl::Seconds(1) / kFiletimeUnit;

static const absl::Time kMinDosFileTime =
    absl::FromCivil(absl::CivilYear(1980), absl::UTCTimeZone());
static const absl::Time kMaxDosFileTime =
    absl::FromCivil(absl::CivilYear(2108), absl::UTCTimeZone()) - kFiletimeUnit;

absl::Duration DurationFromFiletime(const FILETIME& file_time) {
  LONGLONG quadDateTime =
      (static_cast<LONGLONG>(file_time.dwHighDateTime) << 32) |
      static_cast<LONGLONG>(file_time.dwLowDateTime);

  const absl::Duration subsecond =
      kFiletimeUnit * (quadDateTime % kFiletimeUnitsPerSecond);
  const absl::Duration seconds =
      absl::Seconds(quadDateTime / kFiletimeUnitsPerSecond);
  return seconds + subsecond;
}

absl::Time TimeFromFiletime(const FILETIME& file_time) {
  return kFiletimeEpoch + DurationFromFiletime(file_time);
}

bool DurationToFiletime(const absl::Duration duration, FILETIME* file_time) {
  absl::Duration remainder;
  const int64_t filetime_value =
      absl::IDivDuration(duration, kFiletimeUnit, &remainder);
  if (remainder <= -kFiletimeUnit || remainder >= kFiletimeUnit) {
    return false;
  }
  file_time->dwLowDateTime = static_cast<DWORD>(filetime_value);
  file_time->dwHighDateTime = static_cast<DWORD>(filetime_value >> 32);
  return true;
}

void TimeToFiletime(absl::Time time, FILETIME* file_time) {
  // Clamp to the range that FileTimeToDosDateTime supports. Explorer's date
  // display and built-in zip feature can only handle this range.
  time = std::clamp(time, kMinDosFileTime, kMaxDosFileTime);

  if (!DurationToFiletime(time - kFiletimeEpoch, file_time)) {
    DurationToFiletime(kMaxDosFileTime - kFiletimeEpoch, file_time);
  }
}
}  // namespace

// InputFile
std::unique_ptr<IOFile> IOFile::CreateInputFile(absl::string_view file_path) {
  auto file = absl::WrapUnique(new IOFile(file_path));
  file->OpenForRead();
  return file;
}

void IOFile::OpenForRead() {
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
    return;
  }
  total_size_ = file_size.QuadPart;
}

std::unique_ptr<IOFile> IOFile::CreateOutputFile(absl::string_view path) {
  auto file = absl::WrapUnique(new IOFile(path));
  file->OpenForWrite();
  return file;
}

void IOFile::OpenForWrite() {
  // Always open output file path as wide string on Windows platform.
  std::wstring wide_path = string_utils::StringToWideString(path_);
  file_ = ::CreateFileW(wide_path.data(), GENERIC_WRITE, /*dwShareMode=*/0,
                        /*lpSecurityAttributes=*/nullptr, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL,
                        /*hTemplateFile=*/nullptr);
  if (file_ == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to open output file: " << path_
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

absl::Time IOFile::GetLastModifiedTime() const {
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

void IOFile::SetLastModifiedTime(absl::Time last_modified_time) {
  if (file_ == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to set file modified time for: " << path_;
    return;
  }
  FILETIME last_write_time;
  TimeToFiletime(last_modified_time, &last_write_time);
  // Set both creation time and last write time.
  if (::SetFileTime(file_, /*lpCreationTime=*/&last_write_time,
                    /*lpLastAccessTime=*/nullptr, &last_write_time) == 0) {
    LOG(ERROR) << "Failed to set file last write time: " << path_
               << " with error: " << ::GetLastError();
  }
}

}  // namespace nearby::windows
