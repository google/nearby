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

#include "platform/api/input_file.h"
#include "platform/api/output_file.h"
#include "platform/api/platform.h"
#include "platform/base/byte_array.h"
#include "platform/base/exception.h"
#include "platform/base/input_stream.h"
#include "platform/base/output_stream.h"
#include "platform/public/core_config.h"

namespace location {
namespace nearby {

class DLL_API InputFile final {
 public:
  using Platform = api::ImplementationPlatform;
  InputFile(PayloadId payload_id, std::int64_t size);
  ~InputFile();
  InputFile(InputFile&&) noexcept;
  InputFile& operator=(InputFile&&) noexcept;

  // Reads up to size bytes and returns as a ByteArray object wrapped by
  // ExceptionOr.
  // Returns Exception::kIo on error, or end of file.
  ExceptionOr<ByteArray> Read(std::int64_t size);

  // Returns a string that uniqely identifies this file.
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

  // Returns payload id of this file. The closest "file" equivalent is inode.
  PayloadId GetPayloadId() const;

 private:
  std::unique_ptr<api::InputFile> impl_;
  PayloadId id_;
};

class DLL_API OutputFile final {
 public:
  using Platform = api::ImplementationPlatform;
  explicit OutputFile(PayloadId payload_id);
  ~OutputFile();
  OutputFile(OutputFile&&) noexcept;
  OutputFile& operator=(OutputFile&&) noexcept;

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

  // Returns payload id of this file. The closest "file" equivalent is inode.
  PayloadId GetPayloadId() const;

 private:
  std::unique_ptr<api::OutputFile> impl_;
  PayloadId id_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PUBLIC_FILE_H_
