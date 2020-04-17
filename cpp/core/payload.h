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

#include "platform/api/input_file.h"
#include "platform/api/input_stream.h"
#include "platform/byte_array.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

class Payload {
 public:
  struct Type {
    enum Value { UNKNOWN = 0, BYTES = 1, FILE = 2, STREAM = 3 };
  };

  class Stream {
   public:
    Ptr<InputStream> asInputStream() const;

   private:
    template <typename>
    friend class InternalPayloadFactory;
    friend class Payload;

    explicit Stream(Ptr<InputStream> input_stream);
    ScopedPtr<Ptr<InputStream> > input_stream_;
  };

  class File {
   public:
    Ptr<InputFile> asInputFile() const;

   private:
    template <typename>
    friend class InternalPayloadFactory;
    friend class Payload;

    explicit File(const Ptr<InputFile>& input_file);
    ScopedPtr<Ptr<InputFile> > input_file_;
  };

  static Ptr<Payload> fromBytes(ConstPtr<ByteArray> bytes);
  static Ptr<Payload> fromStream(Ptr<InputStream> input_stream);
  static Ptr<Payload> fromFile(const Ptr<InputFile>& input_file);

  ConstPtr<ByteArray> asBytes() const;
  ConstPtr<Stream> asStream() const;
  ConstPtr<File> asFile() const;

  // For when clients of this class want to assume ownership of the
  // ConstPtr<ByteArray> that represents a BYTES Payload.
  ConstPtr<ByteArray> releaseBytes() const;

  std::int64_t getId() const;
  Type::Value getType() const;

 private:
  template <typename>
  friend class InternalPayloadFactory;

  static std::int64_t generateId();

  Payload(std::int64_t id, ConstPtr<ByteArray> bytes);
  Payload(std::int64_t id, ConstPtr<Stream> stream);
  Payload(std::int64_t id, ConstPtr<File> file);

  std::int64_t id_;
  Type::Value type_;
  // This field is mutable because of releaseBytes(), which is just a physically
  // non-const operation that doesn't alter the conceptual const-ness of the
  // Payload object.
  mutable ScopedPtr<ConstPtr<ByteArray> > bytes_;
  ScopedPtr<ConstPtr<File> > file_;
  ScopedPtr<ConstPtr<Stream> > stream_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_PAYLOAD_H_
