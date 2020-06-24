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

#ifndef CORE_V2_PAYLOAD_H_
#define CORE_V2_PAYLOAD_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/input_stream.h"
#include "platform_v2/base/payload_id.h"
#include "platform_v2/base/prng.h"
#include "platform_v2/public/file.h"
#include "absl/types/variant.h"

namespace location {
namespace nearby {
namespace connections {

// Payload is default-constructible, and moveable, but not copyable container
// that holds at most one instance of one of:
// ByteArray, InputStream, or InputFile.
class Payload {
 public:
  using Id = PayloadId;
  // Order of types in variant, and values in Type enum is important.
  // Enum values must match respective variant types.
  using Content = absl::variant<absl::monostate, ByteArray,
                                std::function<InputStream&()>, InputFile>;
  enum class Type { kUnknown = 0, kBytes = 1, kStream = 2, kFile = 3 };

  Payload(Payload&& other) = default;
  ~Payload() = default;
  Payload& operator=(Payload&& other) = default;

  // Default (invalid) payload.
  Payload() : content_(absl::monostate()) {}

  // Constructors for outgoing payloads.
  explicit Payload(ByteArray&& bytes) : content_(std::move(bytes)) {}
  explicit Payload(const ByteArray& bytes) : content_(bytes) {}
  explicit Payload(std::function<InputStream&()> stream)
      : content_(std::move(stream)) {}

  // Constructors for incoming payloads.
  Payload(Id id, ByteArray&& bytes) : content_(std::move(bytes)), id_(id) {}
  Payload(Id id, const ByteArray& bytes) : content_(bytes), id_(id) {}
  Payload(Id id, std::function<InputStream&()> stream)
      : content_(std::move(stream)), id_(id) {}

  // Constructor for incoming and outgoing file payloads.
  Payload(Id id, InputFile file) : content_(std::move(file)), id_(id) {}

  // Returns ByteArray payload, if it has been defined, or empty ByteArray.
  const ByteArray& AsBytes() const& {
    static const ByteArray empty;  // NOLINT: function-level static is OK.
    auto* result = absl::get_if<ByteArray>(&content_);
    return result ? *result : empty;
  }
  ByteArray&& AsBytes() && {
    auto* result = absl::get_if<ByteArray>(&content_);
    return result ? std::move(*result) : std::move(ByteArray());
  }
  // Returns InputStream* payload, if it has been defined, or nullptr.
  InputStream* AsStream() {
    auto* result = absl::get_if<std::function<InputStream&()>>(&content_);
    return result ? &(*result)() : nullptr;
  }
  // Returns InputFile* payload, if it has been defined, or nullptr.
  InputFile* AsFile() { return absl::get_if<InputFile>(&content_); }

  // Returns Payload unique ID.
  Id GetId() const { return id_; }

  // Returns Payload type.
  Type GetType() const { return type_; }

  // Generate Payload Id; to be passed to outgoing file constructor.
  static Id GenerateId() { return Prng().NextInt64(); }

 private:
  Type FindType(const Content& content) const {
    return static_cast<Type>(content_.index());
  }

  Content content_;
  Id id_{GenerateId()};
  Type type_{FindType(content_)};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_PAYLOAD_H_
