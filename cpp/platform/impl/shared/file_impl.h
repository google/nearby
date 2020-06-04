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

#ifndef PLATFORM_IMPL_SHARED_FILE_IMPL_H_
#define PLATFORM_IMPL_SHARED_FILE_IMPL_H_

#include <cstdint>
#include <fstream>

#include "platform/api/input_file.h"
#include "platform/api/output_file.h"
#include "platform/exception.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

class InputFileImpl final : public InputFile {
 public:
  InputFileImpl(const std::string& path, std::int64_t size);
  ~InputFileImpl() override {}

  ExceptionOr<ConstPtr<ByteArray>> read(std::int64_t size) override;
  std::string getFilePath() const override;
  std::int64_t getTotalSize() const override;
  void close() override;

 private:
  std::ifstream file_;
  const std::string path_;
  const std::int64_t total_size_;
};

class OutputFileImpl final : public OutputFile {
 public:
  explicit OutputFileImpl(const std::string& path);
  ~OutputFileImpl() override {}

  Exception::Value write(ConstPtr<ByteArray> data) override;
  void close() override;

 private:
  std::ofstream file_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SHARED_FILE_IMPL_H_
