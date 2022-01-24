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

#include "third_party/nearby/windows/payload_w.h"

#include "core/payload.h"
#include "platform/base/byte_array.h"
#include "platform/base/payload_id.h"

namespace location {
namespace nearby {
namespace windows {

PayloadW::PayloadW() : pImpl(std::make_unique<connections::Payload>()) {}

PayloadW::PayloadW(PayloadW &&other) noexcept : pImpl(std::move(other.pImpl)) {}
//
// PayloadW::~PayloadW() = default;

PayloadW &PayloadW::operator=(PayloadW &&other) noexcept {
  pImpl = std::move(other.pImpl);
  return *this;
}

// Constructors for outgoing payloads.
PayloadW::PayloadW(const char *bytes, const size_t bytes_size)
    : pImpl(std::make_unique<connections::Payload>(
          ByteArray(bytes, bytes_size))) {}

PayloadW::PayloadW(InputFile &&file)
    : pImpl(std::make_unique<connections::Payload>(std::move(file))) {}

PayloadW::PayloadW(std::function<InputStream &()> stream)
    : pImpl(std::make_unique<connections::Payload>(stream)) {}

// Constructors for incoming payloads.
PayloadW::PayloadW(PayloadId id, const char *bytes, const size_t bytes_size)
    : pImpl(std::make_unique<connections::Payload>(
          id, ByteArray(bytes, bytes_size))) {}

PayloadW::PayloadW(PayloadId id, InputFile &&file)
    : pImpl(std::make_unique<connections::Payload>(id, std::move(file))) {}

// TODO(jfcarroll): Handle the case where type is stream.
// PayloadW::PayloadW(PayloadId id, InputStream *stream)
//    : pImpl(std::make_unique<connections::Payload>(id, stream)) {}

// Returns ByteArray payload, if it has been defined, or empty ByteArray.
bool PayloadW::AsBytes(const char *bytes, int &bytes_size) const & {
  auto byteArray = pImpl->AsBytes();
  if (bytes_size < byteArray.size()) {
    bytes_size = byteArray.size();
    bytes = nullptr;
    return false;
  }

  bytes_size = byteArray.size();
  bytes = byteArray.data();
  return true;
}
bool PayloadW::AsBytes(const char *bytes, int &bytes_size) && {
  auto byteArray = pImpl->AsBytes();
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
InputStream *PayloadW::AsStream() { return pImpl->AsStream(); }
// Returns InputFile* payload, if it has been defined, or nullptr.
const InputFile *PayloadW::AsFile() const { return pImpl->AsFile(); }

// Returns Payload unique ID.
uint64_t PayloadW::GetId() const { return pImpl->GetId(); }

// Returns Payload type.
const connections::PayloadType PayloadW::GetType() const {
  return static_cast<connections::PayloadType>(pImpl->GetType());
}

// Sets the payload offset in bytes
void PayloadW::SetOffset(size_t offset) { pImpl->SetOffset(offset); }

size_t PayloadW::GetOffset() { return pImpl->GetOffset(); }

// Generate Payload Id; to be passed to outgoing file constructor.
PayloadId PayloadW::GenerateId() { return connections::Payload::GenerateId(); }

const char *PayloadW::GetParentFolder() const { return nullptr; }

const char *PayloadW::GetFileName() const { return nullptr; }

std::unique_ptr<connections::Payload> PayloadW::GetpImpl() {
  return std::move(pImpl);
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
