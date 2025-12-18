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

#ifndef PLATFORM_IMPL_WINDOWS_FILE_H_
#define PLATFORM_IMPL_WINDOWS_FILE_H_

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/input_file.h"
#include "internal/platform/implementation/output_file.h"

namespace nearby::windows {

class IOFile final : public api::InputFile, public api::OutputFile {
 public:
  static std::unique_ptr<IOFile> CreateInputFile(absl::string_view file_path);

  static std::unique_ptr<IOFile> CreateOutputFile(absl::string_view path);

  ~IOFile() override {
    Close();
  }

  ExceptionOr<ByteArray> Read(std::int64_t size) override;

  std::string GetFilePath() const override { return path_; }

  std::int64_t GetTotalSize() const override { return total_size_; }
  Exception Close() override;

  Exception Write(const ByteArray& data) override;
  absl::Time GetLastModifiedTime() const override;
  void SetLastModifiedTime(absl::Time last_modified_time) override;

 private:
  explicit IOFile(absl::string_view file_path) : path_(file_path) {}
  void OpenForRead();
  void OpenForWrite();

  const std::string path_;
  HANDLE file_ = INVALID_HANDLE_VALUE;
  std::string buffer_;
  std::int64_t total_size_ = 0;
};

}  // namespace nearby::windows

#endif  // PLATFORM_IMPL_WINDOWS_FILE_H_
