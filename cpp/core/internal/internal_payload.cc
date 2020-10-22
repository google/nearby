#include "core/internal/internal_payload.h"

namespace location {
namespace nearby {
namespace connections {

InternalPayload::InternalPayload(Payload payload)
    : payload_(std::move(payload)), payload_id_(payload_.GetId()) {}

Payload InternalPayload::ReleasePayload() {
  return std::move(payload_);
}

Payload::Id InternalPayload::GetId() const { return payload_id_; }

}  // namespace connections
}  // namespace nearby
}  // namespace location
