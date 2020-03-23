#include "core/internal/internal_payload.h"

namespace location {
namespace nearby {
namespace connections {

InternalPayload::InternalPayload(ConstPtr<Payload> payload)
    : payload_(payload), payload_id_(payload_->getId()) {}

InternalPayload::~InternalPayload() {}

ConstPtr<Payload> InternalPayload::releasePayload() {
  return payload_.release();
}

std::int64_t InternalPayload::getId() const { return payload_id_; }

}  // namespace connections
}  // namespace nearby
}  // namespace location
