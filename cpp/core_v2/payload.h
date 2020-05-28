#ifndef CORE_V2_PAYLOAD_H_
#define CORE_V2_PAYLOAD_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/input_stream.h"
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
  // Order of types in variant, and values in Type enum is important.
  // Enum values must match respective variant types.
  using Content =
      absl::variant<absl::monostate, ByteArray, std::unique_ptr<InputStream>,
                    std::unique_ptr<InputFile>>;
  enum class Type { kUnknown = 0, kBytes = 1, kStream = 2, kFile = 3 };

  Payload(Payload&& other) = default;
  ~Payload() = default;
  Payload& operator=(Payload&& other) = default;

  // Create Payload from bytes, steam, or file. Payload is immutable.
  Payload() : content_(absl::monostate()) {}
  explicit Payload(ByteArray&& bytes) : content_(std::move(bytes)) {}
  explicit Payload(const ByteArray& bytes) : content_(bytes) {}
  explicit Payload(std::unique_ptr<InputStream> stream)
      : content_(std::move(stream)) {}
  explicit Payload(std::unique_ptr<InputFile> file)
      : content_(std::move(file)) {}

  // Returns ByteArray payload, if it has been defined, or empty ByteArray.
  const ByteArray& AsBytes() const & {
    static const ByteArray empty; // NOLINT: function-level static is OK.
    auto* result = absl::get_if<ByteArray>(&content_);
    return result ? *result : empty;
  }
  ByteArray&& AsBytes() && {
    auto* result = absl::get_if<ByteArray>(&content_);
    return result ? std::move(*result) : std::move(ByteArray());
  }
  // Returns InputStream* payload, if it has been defined, or nullptr.
  InputStream* AsStream() const {
    auto* result = absl::get_if<std::unique_ptr<InputStream>>(&content_);
    return result ? result->get() : nullptr;
  }
  // Returns InputFile* payload, if it has been defined, or nullptr.
  InputFile* AsFile() const {
    auto* result = absl::get_if<std::unique_ptr<InputFile>>(&content_);
    return result ? result->get() : nullptr;
  }

  // Returns Payload unique ID.
  std::int64_t GetId() const { return id_; }

  // Returns Payload type.
  Type GetType() const { return type_; }

 private:
  static std::int64_t GenerateId() { return Prng().NextInt64(); }
  Type FindType(const Content& content) const {
    return static_cast<Type>(content_.index());
  }

  Content content_;
  std::int64_t id_{GenerateId()};
  Type type_{FindType(content_)};
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_PAYLOAD_H_
