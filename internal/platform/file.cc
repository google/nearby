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

#include "internal/platform/file.h"

namespace location {
namespace nearby {

InputFile::InputFile(PayloadId id, std::int64_t size)
    : impl_(Platform::CreateInputFile(id, size)) {}
InputFile::InputFile(std::string file_path, std::int64_t size)
    : impl_(Platform::CreateInputFile(file_path, size)) {}
InputFile::~InputFile() = default;
InputFile::InputFile(InputFile&& other) noexcept = default;
InputFile& InputFile::operator=(InputFile&& other) = default;

// Reads up to size bytes and returns as a ByteArray object wrapped by
// ExceptionOr.
// Returns Exception::kIo on error, or end of file.
ExceptionOr<ByteArray> InputFile::Read(std::int64_t size) {
  return impl_->Read(size);
}

// Returns a string that uniqely identifies this file.
std::string InputFile::GetFilePath() const { return impl_->GetFilePath(); }

// Returns total size of this file in bytes.
std::int64_t InputFile::GetTotalSize() const { return impl_->GetTotalSize(); }

ExceptionOr<size_t> InputFile::Skip(size_t offset) {
  return impl_->Skip(offset);
}

// Disallows further reads from the file and frees system resources,
// associated with it.
Exception InputFile::Close() { return impl_->Close(); }

// Returns a handle to the underlying input stream.
//
// Returned handle will remain valid even if InputFile is moved, for as long
// as original InputFile lifetime continues.
// Side effects of any non-const operation invoked for InputFile (such as
// Read, or Close will be observable through InputStream& handle, and vice
// versa.
InputStream& InputFile::GetInputStream() { return *impl_; }

OutputFile::OutputFile(std::string file_path)
    : impl_(Platform::CreateOutputFile(file_path)) {}
OutputFile::OutputFile(PayloadId id) : impl_(Platform::CreateOutputFile(id)) {}
OutputFile::~OutputFile() = default;
OutputFile::OutputFile(OutputFile&&) noexcept = default;
OutputFile& OutputFile::operator=(OutputFile&&) = default;

// Writes all data from ByteArray object to the underlying stream.
// Returns Exception::kIo on error, Exception::kSuccess otherwise.
Exception OutputFile::Write(const ByteArray& data) {
  return impl_->Write(data);
}

// Ensures that all data written by previous calls to Write() is passed
// down to the applicable transport layer.
Exception OutputFile::Flush() { return impl_->Flush(); }

// Disallows further writes to the file and frees system resources,
// associated with it.
Exception OutputFile::Close() { return impl_->Close(); }

// Returns a handle to the underlying  output stream.
//
// Returned handle will remain valid even if OutputFile is moved, for as long
// as original OutputFile lifetime continues.
// Side effects of any non-const operation invoked for OutputFile (such as
// Write, or Close will be observable through OutputStream& handle, and vice
// versa.
OutputStream& OutputFile::GetOutputStream() { return *impl_; }

}  // namespace nearby
}  // namespace location
