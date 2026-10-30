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

#include "internal/platform/implementation/windows/output_file.h"

#include <fileapi.h>

#include <algorithm>
#include <cstdint>
#include <limits>

#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/platform/exception.h"
#include "internal/platform/logging.h"

namespace nearby::windows {
namespace {

const absl::Time kFiletimeEpoch =
    absl::FromCivil(absl::CivilYear(1601), absl::UTCTimeZone());

constexpr absl::Duration kFiletimeUnit = absl::Nanoseconds(100);

const absl::Time kMinDosFileTime =
    absl::FromCivil(absl::CivilYear(1980), absl::UTCTimeZone());
const absl::Time kMaxDosFileTime =
    absl::FromCivil(absl::CivilYear(2108), absl::UTCTimeZone()) - kFiletimeUnit;

constexpr wchar_t kZoneIdentifierStreamSuffix[] = L":Zone.Identifier";
constexpr absl::string_view kZoneIdentifierContent =
    "[ZoneTransfer]\r\nZoneId=3\r\n";

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

FILETIME TimeToFiletime(absl::Time time) {
  FILETIME file_time;
  // Clamp to the range that FileTimeToDosDateTime supports. Explorer's date
  // display and built-in zip feature can only handle this range.
  time = std::clamp(time, kMinDosFileTime, kMaxDosFileTime);

  if (!DurationToFiletime(time - kFiletimeEpoch, &file_time)) {
    DurationToFiletime(kMaxDosFileTime - kFiletimeEpoch, &file_time);
  }
  return file_time;
}

}  // namespace

OutputFile::OutputFile(absl::string_view file_path) : path_(file_path) {
  // Always open output file path as wide string on Windows platform.
  std::wstring wide_path = path_.ToWideString();
  file_ = ::CreateFileW(wide_path.data(), GENERIC_WRITE, /*dwShareMode=*/0,
                        /*lpSecurityAttributes=*/nullptr, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL,
                        /*hTemplateFile=*/nullptr);
  if (file_ == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to open output file: " << path_.ToString()
               << " with error: " << ::GetLastError();
    return;
  }
}

Exception OutputFile::Close() {
  if (file_ != INVALID_HANDLE_VALUE) {
    ::CloseHandle(file_);
    file_ = INVALID_HANDLE_VALUE;
  }
  return {Exception::kSuccess};
}

Exception OutputFile::Write(absl::string_view data) {
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
    LOG(ERROR) << "Failed to write file: " << path_.ToString()
               << " with error: " << ::GetLastError();
    return {Exception::kIo};
  }
  return {Exception::kSuccess};
}

void OutputFile::SetLastModifiedTime(absl::Time last_modified_time) {
  if (file_ == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to set file modified time for: " << path_.ToString();
    return;
  }
  FILETIME last_write_time = TimeToFiletime(last_modified_time);
  // Set both creation time and last write time.
  if (::SetFileTime(file_, /*lpCreationTime=*/&last_write_time,
                    /*lpLastAccessTime=*/nullptr, &last_write_time) == 0) {
    LOG(ERROR) << "Failed to set file last write time: " << path_.ToString()
               << " with error: " << ::GetLastError();
  }
}

bool OutputFile::SetOriginMetadata() {
  static constexpr DWORD kShareMode =
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  if (file_ == INVALID_HANDLE_VALUE) {
    // File already closed.
    return false;
  }
  if (!Files::FileExists(path_)) {
    LOG(ERROR) << "File does not exist: " << path_.ToString();
    return false;
  }
  std::wstring ads_file_path =
      path_.ToWideString().append(kZoneIdentifierStreamSuffix);
  HANDLE file_handle =
      ::CreateFileW(ads_file_path.c_str(), GENERIC_WRITE, kShareMode,
                    /*lpSecurityAttributes=*/nullptr, OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, /*hTemplateFile=*/nullptr);
  if (file_handle == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to open zone transfer stream for file: "
               << path_.ToString() << ", " << GetLastError();
    return false;
  }
  DWORD bytes_written = 0;
  if (::WriteFile(file_handle, kZoneIdentifierContent.data(),
                  kZoneIdentifierContent.size(), &bytes_written,
                  /*lpOverlapped=*/nullptr) == 0 ||
      bytes_written < kZoneIdentifierContent.size()) {
    LOG(ERROR) << "Failed to write zone transfer stream for file: "
               << path_.ToString() << ", " << GetLastError();
    ::CloseHandle(file_handle);
    return false;
  }
  ::CloseHandle(file_handle);
  return true;
}

}  // namespace nearby::windows
