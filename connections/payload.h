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

#ifndef CORE_PAYLOAD_H_
#define CORE_PAYLOAD_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "connections/payload_type.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/file.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/payload_id.h"
#include "internal/platform/prng.h"

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
  using Content = std::variant<std::monostate, ByteArray,
                               std::unique_ptr<InputStream>, InputFile>;

  Payload(Payload&& other) noexcept;
  ~Payload();
  Payload& operator=(Payload&& other) noexcept;

  // Default (invalid) payload.
  Payload();

  // Constructors for outgoing payloads.
  explicit Payload(ByteArray&& bytes);
  explicit Payload(const ByteArray& bytes);

  // InputFile is just "a pointer to a file on your disc", a wrapper around a
  // file name or file descriptor. It has no understanding that Nearby is going
  // to create a copy of it on the remote device.
  //
  // FileName and ParentFolder are what Nearby is saying the remote device
  // should save this incoming payload as.
  //
  // Notably, FileName does not have to be respected (you could ask to save it
  // as "photo.png" but instead the recipient saves it as "photo (1).png").
  //
  // ParentFolder must be a relative path, not a full path.

  explicit Payload(std::string parent_folder, std::string file_name,
                   InputFile file);

  explicit Payload(std::unique_ptr<InputStream> stream);

  // Constructors for incoming payloads.
  Payload(Id id, ByteArray&& bytes);
  Payload(Id id, const ByteArray& bytes);
  Payload(Id id, InputFile file);
  Payload(Id id, std::string parent_folder, std::string file_name,
          InputFile input_file);
  Payload(Id id, std::unique_ptr<InputStream> stream);

  // Returns ByteArray payload, if it has been defined, or empty ByteArray.
  const ByteArray& AsBytes() const&;
  // Returns InputStream* payload, if it has been defined, or nullptr.
  InputStream* AsStream();
  // Returns InputFile* payload, if it has been defined, or nullptr.
  InputFile* AsFile();

  // Returns Payload unique ID.
  Id GetId() const;

  // Returns Payload type.
  PayloadType GetType() const;

  // Sets the payload offset in bytes
  void SetOffset(size_t offset);

  size_t GetOffset();

  // Generate Payload Id; to be passed to outgoing file constructor.
  static Id GenerateId();

  // Sets whether the payload is sensitive.
  void SetIsSensitive(bool is_sensitive) { is_sensitive_ = is_sensitive; }
  // Whether the payload is sensitive.
  bool IsSensitive() const { return is_sensitive_; }

  const std::string& GetFileName() const;
  const std::string& GetParentFolder() const;
  absl::Time GetLastModifiedTime() const { return last_modified_time_; }

 private:
  void InitFilePayload(Id id, std::string parent_folder, std::string file_name,
                       InputFile input_file);
  PayloadType FindType() const;

  Id id_{GenerateId()};
  size_t offset_{0};

  std::string parent_folder_;
  std::string file_name_;
  absl::Time last_modified_time_ = absl::Now();

  PayloadType type_{FindType()};
  Content content_;

  bool is_sensitive_ = false;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_PAYLOAD_H_
