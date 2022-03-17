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

#ifndef PLATFORM_IMPL_SHARED_FILE_H_
#define PLATFORM_IMPL_SHARED_FILE_H_

#include <cstdint>
#include <fstream>

#include "absl/strings/string_view.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/input_file.h"
#include "internal/platform/implementation/output_file.h"

namespace location {
namespace nearby {
namespace shared {

class IOFile final : public api::InputFile, public api::OutputFile {
 public:
  static std::unique_ptr<IOFile> CreateInputFile(
      const absl::string_view file_path, size_t size);

  static std::unique_ptr<IOFile> CreateOutputFile(const absl::string_view path);

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  std::string GetFilePath() const override {
    return std::string(path_.data(), path_.size());
  }
  std::int64_t GetTotalSize() const override { return total_size_; }
  Exception Close() override;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override;

 private:
  explicit IOFile(const absl::string_view file_path, size_t size);
  explicit IOFile(const absl::string_view file_path);

  std::fstream file_;
  std::string path_;
  std::int64_t total_size_;
};

}  // namespace shared
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SHARED_FILE_H_
