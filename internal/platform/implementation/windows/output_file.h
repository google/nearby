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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_OUTPUT_FILE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_OUTPUT_FILE_H_

#include <windows.h>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/base/file_path.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/output_file.h"

namespace nearby::windows {

class OutputFile final : public api::OutputFile {
 public:
  explicit OutputFile(absl::string_view file_path);

  ~OutputFile() override { Close(); }

  Exception Close() override;

  nearby::FilePath GetFilePath() const override { return path_; }
  Exception Write(absl::string_view data) override;
  void SetLastModifiedTime(absl::Time last_modified_time) override;
  bool SetOriginMetadata() override;

 private:
  const nearby::FilePath path_;
  HANDLE file_ = INVALID_HANDLE_VALUE;
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_OUTPUT_FILE_H_
