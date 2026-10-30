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

#ifndef PLATFORM_API_OUTPUT_FILE_H_
#define PLATFORM_API_OUTPUT_FILE_H_

#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "internal/platform/exception.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace api {

// An OutputFile represents a writable file on the system.
class OutputFile : public OutputStream {
 public:
  OutputFile(OutputFile&& other) = delete;
  OutputFile& operator=(OutputFile&& other) = delete;
  OutputFile(const OutputFile&) = delete;
  OutputFile& operator=(const OutputFile&) = delete;

  OutputFile() = default;
  ~OutputFile() override = default;
  virtual FilePath GetFilePath() const = 0;
  virtual void SetLastModifiedTime(absl::Time last_modified_time) = 0;

  // Sets the origin metadata for the file as externally downloaded so any
  // security policies can be applied.
  // Returns true if the origin metadata was set successfully.
  virtual bool SetOriginMetadata() { return false; }
  // File flush is a no-op.
  Exception Flush() override { return {Exception::kSuccess}; }
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_OUTPUT_FILE_H_
