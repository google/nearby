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
#include "internal/platform/payload_id.h"

namespace location {
namespace nearby {
class InputFile;
struct InputFileDeleter {
  void operator()(InputFile* p) { delete p; }
};

class OutputFile;
struct OutputFileDeleter {
  void operator()(OutputFile* p) { delete p; }
};

}  // namespace nearby
}  // namespace location

namespace location {
namespace nearby {
namespace windows {

class DLL_API InputFileW {
 public:
  explicit InputFileW(location::nearby::InputFile* input_file);
  InputFileW(location::nearby::PayloadId payload_id, size_t size);
  InputFileW(const char* file_path, size_t size);
  InputFileW(InputFileW&&) noexcept;

  // Returns a string that uniquely identifies this file.
  void GetFilePath(char* file_path) const;

  // Returns total size of this file in bytes.
  size_t GetTotalSize() const;

  std::unique_ptr<location::nearby::InputFile,
                  location::nearby::InputFileDeleter>
  GetImpl();

 private:
  std::unique_ptr<location::nearby::InputFile,
                  location::nearby::InputFileDeleter>
      impl_;
};

class DLL_API OutputFileW {
 public:
  explicit OutputFileW(location::nearby::PayloadId payload_id);
  explicit OutputFileW(const char* file_path);
  OutputFileW(OutputFileW&&) noexcept;
  OutputFileW& operator=(OutputFileW&&) noexcept;

  std::unique_ptr<location::nearby::OutputFile,
                  location::nearby::OutputFileDeleter>
  GetImpl();

 private:
  std::unique_ptr<location::nearby::OutputFile,
                  location::nearby::OutputFileDeleter>
      impl_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_CLIENTS_WINDOWS_FILE_W_H_
