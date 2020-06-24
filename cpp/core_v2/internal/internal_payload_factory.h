#ifndef CORE_V2_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_
#define CORE_V2_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_

#include "core_v2/internal/internal_payload.h"
#include "core_v2/payload.h"
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

#endif  // CORE_V2_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_
