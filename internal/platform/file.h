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

#ifndef PLATFORM_PUBLIC_FILE_H_
#define PLATFORM_PUBLIC_FILE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/input_file.h"
#include "internal/platform/implementation/output_file.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {

class InputFile final {
 public:
  using Platform = api::ImplementationPlatform;
  InputFile(PayloadId payload_id, std::int64_t size);
  InputFile(std::string file_path, std::int64_t size);
  ~InputFile();
  InputFile(InputFile&&) noexcept;
  InputFile& operator=(InputFile&&);

  // Reads up to size bytes and returns as a ByteArray object wrapped by
  // ExceptionOr.
  // Returns Exception::kIo on error, or end of file.
  ExceptionOr<ByteArray> Read(std::int64_t size);

  // Returns a string that uniquely identifies this file.
  std::string GetFilePath() const;

  // Returns total size of this file in bytes.
  std::int64_t GetTotalSize() const;

  ExceptionOr<size_t> Skip(size_t offset);

  // Disallows further reads from the file and frees system resources,
  // associated with it.
  Exception Close();

  // Returns a handle to the underlying input stream.
  //
  // Returned handle will remain valid even if InputFile is moved, for as long
  // as original InputFile lifetime continues.
  // Side effects of any non-const operation invoked for InputFile (such as
  // Read, or Close will be observable through InputStream& handle, and vice
  // versa.
  InputStream& GetInputStream();

 private:
  std::unique_ptr<api::InputFile> impl_;
};

class OutputFile final {
 public:
  using Platform = api::ImplementationPlatform;
  explicit OutputFile(PayloadId payload_id);
  explicit OutputFile(std::string file_path);
  ~OutputFile();
  OutputFile(OutputFile&&) noexcept;
  OutputFile& operator=(OutputFile&&);

  // Writes all data from ByteArray object to the underlying stream.
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Write(const ByteArray& data);

  // Ensures that all data written by previous calls to Write() is passed
  // down to the applicable transport layer.
  Exception Flush();

  // Disallows further writes to the file and frees system resources,
  // associated with it.
  Exception Close();

  // Returns a handle to the underlying  output stream.
  //
  // Returned handle will remain valid even if OutputFile is moved, for as long
  // as original OutputFile lifetime continues.
  // Side effects of any non-const operation invoked for OutputFile (such as
  // Write, or Close will be observable through OutputStream& handle, and vice
  // versa.
  OutputStream& GetOutputStream();

 private:
  std::unique_ptr<api::OutputFile> impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_FILE_H_
