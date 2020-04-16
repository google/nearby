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

#ifndef CORE_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_
#define CORE_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_

#include "core/internal/internal_payload.h"
#include "core/payload.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

template <typename Platform>
class InternalPayloadFactory {
 public:
  // Creates an InternalPayload representing an outgoing Payload.
  //
  // The returned Ptr<InternalPayload> will take ownership of the passed-in
  // 'payload'.
  Ptr<InternalPayload> createOutgoing(ConstPtr<Payload> payload);

  // Creates an InternalPayload representing an incoming Payload from a remote
  // endpoint.
  Ptr<InternalPayload> createIncoming(
      const PayloadTransferFrame& payload_transfer_frame);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/internal_payload_factory.cc"

#endif  // CORE_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_
