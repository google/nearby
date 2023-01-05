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

#include <string>

#include <memory>

#include "connections/implementation/internal_payload.h"
#include "connections/payload.h"

namespace nearby {
namespace connections {

// Creates an InternalPayload representing an outgoing Payload.
std::unique_ptr<InternalPayload> CreateOutgoingInternalPayload(Payload payload);

// Creates an InternalPayload representing an incoming Payload from a remote
// endpoint.
std::unique_ptr<InternalPayload> CreateIncomingInternalPayload(
    const location::nearby::connections::PayloadTransferFrame& frame,
    const std::string& custom_save_path);

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_INTERNAL_PAYLOAD_FACTORY_H_
