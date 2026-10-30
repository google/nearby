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

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "internal/base/files.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/input_file.h"
#include "internal/platform/implementation/output_file.h"

namespace nearby::shared {

namespace {
class InputFileImpl : public api::InputFile {
 public:
  explicit InputFileImpl(absl::string_view file_path) : path_(file_path) {
    file_ =
        std::fstream(path_, std::ios::binary | std::ios::in | std::ios::ate);
    total_size_ = Files::GetFileSize(FilePath(path_)).value_or(0);
    file_.seekg(0);
  }

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

  std::string GetFilePath() const override { return path_; };
  std::int64_t GetTotalSize() const override { return total_size_; };
  absl::Time GetLastModifiedTime() const override { return absl::Now(); };

 private:
  const std::string path_;
  std::fstream file_;
  std::int64_t total_size_ = 0;
};

class OutputFileImpl : public api::OutputFile {
 public:
  explicit OutputFileImpl(absl::string_view file_path) : path_(file_path) {
    file_.open(path_.ToString(), std::ios::binary | std::ios::out);
  }

  FilePath GetFilePath() const override { return path_; };
  void SetLastModifiedTime(absl::Time last_modified_time) override {};
  Exception Write(absl::string_view data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  const nearby::FilePath path_;
  std::fstream file_;
};

ExceptionOr<ByteArray> InputFileImpl::Read(std::int64_t size) {
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

Exception InputFileImpl::Close() {
  if (file_.is_open()) {
    file_.close();
  }
  return {Exception::kSuccess};
}

Exception OutputFileImpl::Write(absl::string_view data) {
  if (!file_.is_open()) {
    return {Exception::kIo};
  }

  if (!file_.good()) {
    return {Exception::kIo};
  }

  file_.write(data.data(), data.size());
  return {file_.good() ? Exception::kSuccess : Exception::kIo};
}

Exception OutputFileImpl::Flush() {
  if (!file_.is_open()) {
    return {Exception::kIo};
  }
  file_.flush();
  return {file_.good() ? Exception::kSuccess : Exception::kIo};
}

Exception OutputFileImpl::Close() {
  if (!file_.is_open()) {
    return {Exception::kSuccess};
  }
  // Only report failures from this call (final flush or OS close). An earlier
  // write error has already been returned by Write().
  const bool was_good = file_.good();
  file_.close();
  return {was_good && file_.fail() ? Exception::kIo : Exception::kSuccess};
}
}  // namespace

std::unique_ptr<api::InputFile> IOFile::CreateInputFile(
    const absl::string_view file_path) {
  return std::make_unique<InputFileImpl>(file_path);
}

std::unique_ptr<api::OutputFile> IOFile::CreateOutputFile(
    const absl::string_view path) {
  return std::make_unique<OutputFileImpl>(path);
}

}  // namespace nearby::shared
