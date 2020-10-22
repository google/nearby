#ifndef CORE_PAYLOAD_H_
#define CORE_PAYLOAD_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

#include "platform/base/byte_array.h"
#include "platform/base/input_stream.h"
#include "platform/base/payload_id.h"
#include "platform/base/prng.h"
#include "platform/public/file.h"
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

#endif  // CORE_PAYLOAD_H_
