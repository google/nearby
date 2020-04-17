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

#include "core/payload.h"

#include <cstdlib>
#include <limits>

#include "platform/prng.h"

namespace location {
namespace nearby {
namespace connections {

////////////////////////////////// Payload //////////////////////////////////

Ptr<Payload> Payload::fromBytes(ConstPtr<ByteArray> bytes) {
  return MakePtr(new Payload(generateId(), bytes));
}

Ptr<Payload> Payload::fromStream(Ptr<InputStream> input_stream) {
  return MakePtr(
      new Payload(generateId(), MakeConstPtr(new Stream(input_stream))));
}

Ptr<Payload> Payload::fromFile(const Ptr<InputFile>& input_file) {
  return MakePtr(new Payload(generateId(), MakeConstPtr(new File(input_file))));
}

ConstPtr<ByteArray> Payload::asBytes() const { return bytes_.get(); }

ConstPtr<Payload::Stream> Payload::asStream() const { return stream_.get(); }

ConstPtr<Payload::File> Payload::asFile() const { return file_.get(); }

ConstPtr<ByteArray> Payload::releaseBytes() const { return bytes_.release(); }

std::int64_t Payload::getId() const { return id_; }

Payload::Type::Value Payload::getType() const { return type_; }

std::int64_t Payload::generateId() { return Prng().nextInt64(); }

Payload::Payload(std::int64_t id, ConstPtr<ByteArray> bytes)
    : id_(id),
      type_(Type::BYTES),
      bytes_(std::move(bytes)),
      file_(ConstPtr<File>()),
      stream_(ConstPtr<Stream>()) {}

Payload::Payload(std::int64_t id, ConstPtr<File> file)
    : id_(id),
      type_(Type::FILE),
      bytes_(ConstPtr<ByteArray>()),
      file_(std::move(file)),
      stream_(ConstPtr<Stream>()) {}

Payload::Payload(std::int64_t id, ConstPtr<Stream> stream)
    : id_(id),
      type_(Type::STREAM),
      bytes_(ConstPtr<ByteArray>()),
      file_(ConstPtr<File>()),
      stream_(stream) {}

//////////////////////////// Payload::File ////////////////////////////////

Ptr<InputFile> Payload::File::asInputFile() const { return input_file_.get(); }

Payload::File::File(const Ptr<InputFile>& input_file)
    : input_file_(input_file) {}

//////////////////////////// Payload::Stream ////////////////////////////////

Ptr<InputStream> Payload::Stream::asInputStream() const {
  return input_stream_.get();
}

Payload::Stream::Stream(Ptr<InputStream> input_stream)
    : input_stream_(input_stream) {}

}  // namespace connections
}  // namespace nearby
}  // namespace location
