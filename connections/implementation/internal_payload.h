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

#ifndef CORE_INTERNAL_INTERNAL_PAYLOAD_H_
#define CORE_INTERNAL_INTERNAL_PAYLOAD_H_

#include <cstdint>

#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/payload.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace connections {

// Defines the operations layered atop a Payload, for use inside the
// OfflineServiceController.
//
// <p>There will be an extension of this abstract base class per type of
// Payload.
class InternalPayload {
 public:
  static constexpr int kIndeterminateSize = -1;

  explicit InternalPayload(Payload payload);
  virtual ~InternalPayload() = default;

  Payload ReleasePayload();

  Payload::Id GetId() const;

  const std::string& GetParentFolder() { return payload_.GetParentFolder(); }
  const std::string& GetFileName() { return payload_.GetFileName(); }

  // Returns the PayloadType of the Payload to which this object is bound.
  //
  // <p>Note that this is supposed to return the type from the OfflineFrame
  // proto rather than what is already available via
  // Payload::getType().
  //
  // @return The PayloadType.
  virtual location::nearby::connections::PayloadTransferFrame::PayloadHeader::
      PayloadType
      GetType() const = 0;

  // Deduces the total size of the Payload to which this object is bound.
  //
  // @return The total size, or -1 if it cannot be deduced (for example, when
  // dealing with streaming data).
  virtual std::int64_t GetTotalSize() const = 0;

  // Breaks off the next chunk from the Payload to which this object is bound.
  //
  // <p>Used when we have a complete Payload that we want to break into smaller
  // byte blobs for sending across a hard boundary (like the other side of
  // a Binder, or another device altogether).
  //
  // @param chunk_size The preferred size of the next chunk. Depending on
  // payload type, the provided size may be ignored.
  // @return The next chunk from the Payload, or null if we've reached the end.
  virtual ByteArray DetachNextChunk(int chunk_size) = 0;

  // Adds the next chunk that comprises the Payload to which this object is
  // bound.
  //
  // <p>Used when we are trying to reconstruct a Payload that lives on the
  // other side of a hard boundary (like the other side of a Binder, or another
  // device altogether), one byte blob at a time.
  //
  // @param chunk The next chunk; this being null signals that this is the last
  // chunk, which will typically be used as a trigger to perform whatever state
  // cleanup may be required by the concrete implementation.
  virtual Exception AttachNextChunk(const ByteArray& chunk) = 0;

  // Skips current stream pointer to the offset.
  //
  // Used when this is a resume outgoing transfer, so we want to skip
  // some data until the offset position.
  //
  // @return the offset really skipped
  virtual ExceptionOr<size_t> SkipToOffset(size_t offset) = 0;

  // Cleans up any resources used by this Payload. Called when we're stopping
  // early, e.g. after being cancelled or having no more recipients left.
  virtual void Close() {}

 protected:
  Payload payload_;
  // We're caching the payload ID here because the backing payload will be
  // released to another owner during the lifetime of an incoming
  // InternalPayload.
  Payload::Id payload_id_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_INTERNAL_PAYLOAD_H_
