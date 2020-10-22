#ifndef CORE_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_
#define CORE_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_

#include "core/internal/internal_payload.h"
#include "core/payload.h"
#include "proto/connections/offline_wire_formats.pb.h"

namespace location {
namespace nearby {
namespace connections {

// Creates an InternalPayload representing an outgoing Payload.
std::unique_ptr<InternalPayload> CreateOutgoingInternalPayload(Payload payload);

// Creates an InternalPayload representing an incoming Payload from a remote
// endpoint.
std::unique_ptr<InternalPayload> CreateIncomingInternalPayload(
    const PayloadTransferFrame& frame);

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_
