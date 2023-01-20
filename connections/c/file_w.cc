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
#include "connections/c/file_w.h"

#include <string>

#include "internal/platform/file.h"

namespace nearby {
void InputFileDeleter::operator()(nearby::InputFile* p) { delete p; }
void OutputFileDeleter::operator()(nearby::OutputFile* p) { delete p; }

namespace windows {
InputFileW::InputFileW(InputFile* input_file)
    : impl_(std::unique_ptr<nearby::InputFile, nearby::InputFileDeleter>(
          new nearby::InputFile(std::move(*input_file)))) {}
InputFileW::InputFileW(PayloadId payload_id, size_t size)
    : impl_(std::unique_ptr<nearby::InputFile, nearby::InputFileDeleter>(
          new nearby::InputFile(payload_id, size))) {}
InputFileW::InputFileW(const char* file_path, size_t size)
    : impl_(std::unique_ptr<nearby::InputFile, nearby::InputFileDeleter>(
          new nearby::InputFile(file_path, size))) {}
InputFileW::InputFileW(InputFileW&& other) noexcept
    : impl_(std::move(other.impl_)) {}

// Returns a string that uniquely identifies this file.
// Caller allocates buffer[MAX_PATH] and is responsible
// for freeing.
void InputFileW::GetFilePath(char* file_path) const {
  std::string fp = impl_->GetFilePath();
  strncpy(file_path, fp.c_str(), fp.length());
}

// Returns total size of this file in bytes.
size_t InputFileW::GetTotalSize() const { return impl_->GetTotalSize(); }

std::unique_ptr<nearby::InputFile, nearby::InputFileDeleter>
InputFileW::GetImpl() {
  return std::move(impl_);
}

OutputFileW::OutputFileW(PayloadId payload_id) {}
OutputFileW::OutputFileW(const char* file_path) {}
OutputFileW::OutputFileW(OutputFileW&&) noexcept {}
OutputFileW& OutputFileW::operator=(OutputFileW&& other) noexcept {
  impl_ = std::move(other.impl_);
  return *this;
}

std::unique_ptr<nearby::OutputFile, nearby::OutputFileDeleter>
OutputFileW::GetImpl() {
  return std::move(impl_);
}

}  // namespace windows
}  // namespace nearby
