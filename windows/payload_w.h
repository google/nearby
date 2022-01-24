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

#ifndef WINDOWS_PAYLOAD_H_
#define WINDOWS_PAYLOAD_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include "core/payload_type.h"
#include "platform/base/payload_id.h"
#include "platform/public/core_config.h"

namespace location {
namespace nearby {
namespace connections {
class Payload;
}  // namespace connections
class InputFile;
class InputStream;
namespace windows {

// Payload is default-constructible, and moveable, but not copyable container
// that holds at most one instance of one of:
// ByteArray, InputStream, or InputFile.
class DLL_API PayloadW {
 public:
  PayloadW(PayloadW&& other) noexcept;
  PayloadW& operator=(PayloadW&& other) noexcept;

  // Default (invalid) payload.
  PayloadW();
  ~PayloadW() {}

  // Constructors for outgoing payloads.
  explicit PayloadW(const char* bytes, const size_t size);

  explicit PayloadW(InputFile&& file);
  explicit PayloadW(std::function<InputStream&()> stream);

  // Constructors for incoming payloads.
  PayloadW(PayloadId id, const char* bytes, const size_t size);
  PayloadW(PayloadId id, InputFile&& file);
  PayloadW(PayloadId id, InputStream* stream);
  // Returns ByteArray payload, if it has
  // been defined, or empty ByteArray.
  bool AsBytes(const char* bytes, int& bytes_size) const&;
  bool AsBytes(const char* bytes, int& bytes_size) &&;
  // Returns InputStream* payload, if it has been defined, or nullptr.
  InputStream* AsStream();
  // Returns InputFile* payload, if it has been defined, or nullptr.
  const InputFile* AsFile() const;

  // Returns Payload unique ID.
  uint64_t GetId() const;

  // Returns Payload type.
  const connections::PayloadType GetType() const;

  // Sets the payload offset in bytes
  void SetOffset(size_t offset);

  size_t GetOffset();

  // Generate Payload Id; to be passed to outgoing file constructor.
  static PayloadId GenerateId();

  const char* GetFileName() const;
  const char* GetParentFolder() const;

  std::unique_ptr<connections::Payload> GetpImpl();

 private:
  std::unique_ptr<connections::Payload> pImpl;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // WINDOWS_PAYLOAD_H_
