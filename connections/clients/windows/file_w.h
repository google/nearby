// Copyright 2022 Google LLC
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
#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_FILE_W_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_FILE_W_H_

#include <memory>
#include <string>

#include "connections/clients/windows/dll_config.h"
#include "internal/platform/file.h"
#include "internal/platform/payload_id.h"

namespace location::nearby::windows {

class DLL_API InputFileW {
 public:
  InputFileW(InputFile* input_file);
  InputFileW(PayloadId payload_id, size_t size);
  InputFileW(std::string file_path, size_t size);
  InputFileW(InputFileW&&) noexcept;

  // Returns a string that uniquely identifies this file.
  std::string GetFilePath() const;

  // Returns total size of this file in bytes.
  size_t GetTotalSize() const;

  std::unique_ptr<nearby::InputFile, nearby::InputFileDeleter> GetImpl();

 private:
  std::unique_ptr<nearby::InputFile, nearby::InputFileDeleter> impl_;
};

class DLL_API OutputFileW {
 public:
  explicit OutputFileW(PayloadId payload_id);
  explicit OutputFileW(std::string file_path);
  OutputFileW(OutputFileW&&) noexcept;
  OutputFileW& operator=(OutputFileW&&) noexcept;

  std::unique_ptr<nearby::OutputFile, nearby::OutputFileDeleter> GetImpl();

 private:
  std::unique_ptr<nearby::OutputFile, nearby::OutputFileDeleter> impl_;
};

}  // namespace location::nearby::windows

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_FILE_W_H_
