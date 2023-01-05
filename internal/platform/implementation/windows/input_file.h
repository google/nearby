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

#ifndef PLATFORM_IMPL_WINDOWS_INPUT_FILE_H_
#define PLATFORM_IMPL_WINDOWS_INPUT_FILE_H_

#include "internal/platform/implementation/input_file.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace windows {

// An InputFile represents a readable file on the system.
class InputFile : public api::InputFile {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~InputFile() override = default;
  // TODO(b/184975123): replace with real implementation.
  std::string GetFilePath() const override { return "Un-implemented"; }
  // TODO(b/184975123): replace with real implementation.
  std::int64_t GetTotalSize() const override { return 0; }

  // throws Exception::kIo
  // TODO(b/184975123): replace with real implementation.
  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    return ExceptionOr<ByteArray>(Exception::kFailed);
  }
  // throws Exception::kIo
  // TODO(b/184975123): replace with real implementation.
  Exception Close() override { return Exception{}; }
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_INPUT_FILE_H_
