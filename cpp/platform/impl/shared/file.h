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
#include "platform/api/input_file.h"
#include "platform/api/output_file.h"
#include "platform/base/exception.h"

namespace location {
namespace nearby {
namespace shared {

class InputFile final : public api::InputFile {
 public:
  explicit InputFile(const char* file_path);
  ~InputFile() override = default;
  InputFile(InputFile&&) = default;
  InputFile& operator=(InputFile&&) = default;

  ExceptionOr<ByteArray> Read(int64_t size) override;
  std::string GetFilePath() const override { return file_path_; }
  int64_t GetTotalSize() const override { return total_size_; }
  Exception Close() override;

 private:
  std::ifstream file_;
  std::string file_path_;
  int64_t total_size_;
};

class OutputFile final : public api::OutputFile {
 public:
  explicit OutputFile(const char* file_path);
  ~OutputFile() override = default;
  OutputFile(OutputFile&&) = default;
  OutputFile& operator=(OutputFile&&) = default;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  std::ofstream file_;
};

}  // namespace shared
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SHARED_FILE_H_
