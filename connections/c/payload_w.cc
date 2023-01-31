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
#include "connections/c/payload_w.h"

#include "connections/c/file_w.h"
#include "connections/payload.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/payload_id.h"

namespace nearby {
// Must implement Deleter since Payload wasn't fully defined in
// the header
namespace connections {
class Payload;
void PayloadDeleter::operator()(connections::Payload *p) { delete p; }

}  // namespace connections
namespace windows {

PayloadW::PayloadW()
    : impl_(std::unique_ptr<connections::Payload, connections::PayloadDeleter>(
          new connections::Payload())) {}

PayloadW::~PayloadW() {}
PayloadW::PayloadW(PayloadW &&other) noexcept : impl_(std::move(other.impl_)) {}

PayloadW &PayloadW::operator=(PayloadW &&other) noexcept {
  impl_ = std::move(other.impl_);
  return *this;
}

// Constructors for outgoing payloads.
PayloadW::PayloadW(const char *bytes, const size_t bytes_size)
    : impl_(std::unique_ptr<connections::Payload, connections::PayloadDeleter>(
          new connections::Payload(ByteArray(bytes, bytes_size)))) {}

PayloadW::PayloadW(InputFileW &file)
    : impl_(std::unique_ptr<connections::Payload, connections::PayloadDeleter>(
          new connections::Payload(InputFile(std::move(*file.GetImpl()))))) {}

// TODO(jfcarroll): Convert std::function to function pointer
PayloadW::PayloadW(std::function<InputStream &()> stream)
    : impl_(std::unique_ptr<connections::Payload, connections::PayloadDeleter>(
          new connections::Payload(stream))) {}

// Constructors for incoming payloads.
PayloadW::PayloadW(PayloadId id, const char *bytes, const size_t bytes_size)
    : impl_(std::unique_ptr<connections::Payload, connections::PayloadDeleter>(
          new connections::Payload(id, ByteArray(bytes, bytes_size)))) {}

PayloadW::PayloadW(PayloadId id, InputFileW file)
    : impl_(std::unique_ptr<connections::Payload, connections::PayloadDeleter>(
          new connections::Payload(id, std::move(*file.GetImpl())))) {}

PayloadW::PayloadW(const char *parent_folder, const char *file_name,
                   InputFileW file)
    : impl_(std::unique_ptr<connections::Payload, connections::PayloadDeleter>(
          new connections::Payload(parent_folder, file_name,
                                   std::move(*file.GetImpl())))) {}

PayloadW::PayloadW(PayloadId id, std::function<InputStream &()> stream)
    : impl_(std::unique_ptr<connections::Payload, connections::PayloadDeleter>(
          new connections::Payload(id, stream))) {}

// Returns ByteArray payload, if it has been defined, or empty ByteArray.
bool PayloadW::AsBytes(const char *&bytes, size_t &bytes_size) const & {
  auto byteArray = impl_->AsBytes();
  if (bytes_size < byteArray.size()) {
    bytes_size = byteArray.size();
    bytes = nullptr;
    return false;
  }

  bytes_size = byteArray.size();
  bytes = byteArray.data();
  return true;
}
bool PayloadW::AsBytes(const char *&bytes, size_t &bytes_size) && {
  auto byteArray = impl_->AsBytes();
  if (bytes_size < byteArray.size()) {
    bytes_size = byteArray.size();
    bytes = nullptr;
    return false;
  }

  bytes_size = byteArray.size();
  bytes = byteArray.data();
  return true;
}
// Returns InputStream* payload, if it has been defined, or nullptr.
InputStream *PayloadW::AsStream() { return impl_->AsStream(); }
// Returns InputFile* payload, if it has been defined, or nullptr.
InputFile *PayloadW::AsFile() const { return impl_->AsFile(); }

// Returns Payload unique ID.
int64_t PayloadW::GetId() const { return impl_->GetId(); }

// Returns Payload type.
const connections::PayloadType PayloadW::GetType() const {
  return static_cast<connections::PayloadType>(impl_->GetType());
}

// Sets the payload offset in bytes
void PayloadW::SetOffset(size_t offset) { impl_->SetOffset(offset); }

size_t PayloadW::GetOffset() { return impl_->GetOffset(); }

// Generate Payload Id; to be passed to outgoing file constructor.
PayloadId PayloadW::GenerateId() { return connections::Payload::GenerateId(); }

const char *PayloadW::GetParentFolder() const { return nullptr; }

const char *PayloadW::GetFileName() const { return nullptr; }

std::unique_ptr<connections::Payload, connections::PayloadDeleter>
PayloadW::GetImpl() {
  return std::move(impl_);
}

}  // namespace windows
}  // namespace nearby
