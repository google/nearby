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

#include "connections/payload.h"

#include <algorithm>
#include <string>

namespace nearby {
namespace connections {

namespace {

std::string getFileName(const std::string& s) {
  std::string s_copy(s);
  std::replace(s_copy.begin(), s_copy.end(), '\\',
               '/');  // replace all '\\' to '/'

  size_t lastForwardSepIndex = s_copy.rfind('/', s.length());

  return s_copy.substr(lastForwardSepIndex + 1,
                       s_copy.length() - lastForwardSepIndex);
}

}  // namespace

// Payload is default-constructible, and moveable, but not copyable container
// that holds at most one instance of one of:
// ByteArray, InputStream, or InputFile.
Payload::Payload(Payload&& other) noexcept = default;
Payload::~Payload() = default;
Payload& Payload::operator=(Payload&& other) noexcept = default;

// Default (invalid) payload.
Payload::Payload()
    : type_(PayloadType::kUnknown), content_(absl::monostate()) {}

// Constructors for outgoing payloads.
Payload::Payload(ByteArray&& bytes)
    : type_(PayloadType::kBytes), content_(bytes) {}

Payload::Payload(const ByteArray& bytes)
    : type_(PayloadType::kBytes), content_(bytes) {}

Payload::Payload(InputFile input_file)
    : id_(std::hash<std::string>()(input_file.GetFilePath())),
      file_name_(getFileName(input_file.GetFilePath())),
      type_(PayloadType::kFile),
      content_(std::move(input_file)) {}

Payload::Payload(Id id, InputFile input_file)
    : id_(id),
      file_name_(getFileName(input_file.GetFilePath())),
      type_(PayloadType::kFile),
      content_(std::move(input_file)) {}

Payload::Payload(std::string parent_folder, std::string file_name,
                 InputFile input_file)
    : id_(std::hash<std::string>()(input_file.GetFilePath())),
      parent_folder_(parent_folder),
      file_name_(file_name),
      type_(PayloadType::kFile),
      content_(std::move(input_file)) {}

Payload::Payload(std::function<InputStream&()> stream)
    : type_(PayloadType::kStream), content_(std::move(stream)) {}

// Constructors for incoming payloads.
Payload::Payload(Id id, ByteArray&& bytes)
    : id_(id), type_(PayloadType::kBytes), content_(std::move(bytes)) {}

Payload::Payload(Id id, const ByteArray& bytes)
    : id_(id), type_(PayloadType::kBytes), content_(bytes) {}

Payload::Payload(Id id, std::string parent_folder, std::string file_name,
                 InputFile input_file)
    : id_(id),
      parent_folder_(parent_folder),
      file_name_(file_name),
      type_(PayloadType::kFile),
      content_(std::move(input_file)) {}

Payload::Payload(Id id, std::function<InputStream&()> stream)
    : id_(id), type_(PayloadType::kStream), content_(std::move(stream)) {}

// Returns ByteArray payload, if it has been defined, or empty ByteArray.
const ByteArray& Payload::AsBytes() const& {
  static const ByteArray empty;  // NOLINT: function-level static is OK.
  auto* result = absl::get_if<ByteArray>(&content_);
  return result ? *result : empty;
}
// Returns InputStream* payload, if it has been defined, or nullptr.
InputStream* Payload::AsStream() {
  auto* result = absl::get_if<std::function<InputStream&()>>(&content_);
  return result ? &(*result)() : nullptr;
}
// Returns InputFile* payload, if it has been defined, or nullptr.
InputFile* Payload::AsFile() { return absl::get_if<InputFile>(&content_); }

// Returns Payload unique ID.
Payload::Id Payload::GetId() const { return id_; }

// Returns Payload type.
PayloadType Payload::GetType() const { return type_; }

// Sets the payload offset in bytes
void Payload::SetOffset(size_t offset) {
  CHECK(type_ == PayloadType::kFile || type_ == PayloadType::kStream);
  InputFile* file = AsFile();
  if (file != nullptr) {
    CHECK(file->GetTotalSize() > 0 && offset < (size_t)file->GetTotalSize());
  }
  offset_ = offset;
}

size_t Payload::GetOffset() { return offset_; }

// Generate Payload Id; to be passed to outgoing file constructor.
Payload::Id Payload::GenerateId() { return Prng().NextInt64(); }

PayloadType Payload::FindType() const {
  return static_cast<PayloadType>(content_.index());
}

const std::string& Payload::GetParentFolder() const { return parent_folder_; }

const std::string& Payload::GetFileName() const { return file_name_; }

}  // namespace connections
}  // namespace nearby
